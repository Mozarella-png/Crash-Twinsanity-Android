// The PS2 side's asm statements run from their text (src/platform/ps2/*.cpp, made into calls of Ref::Asm by
// tools/make_reference.py): the EE's instructions the maths' asm uses (128 bit registers; the addresses are the host's), the COP2
// macro instructions assembled into their words and run on the reference VU0, VCALLMS running the microcode

#include "refvu0.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace Ref
{
Operand In(const void* pointer)
{
    return {Operand::Integer, reinterpret_cast<u64>(pointer), 0.0f, nullptr};
}

Operand In(s32 value)
{
    return {Operand::Integer, static_cast<u64>(static_cast<s64>(value)), 0.0f, nullptr};
}

Operand In(u32 value)
{
    return {Operand::Integer, static_cast<u64>(static_cast<s64>(static_cast<s32>(value))), 0.0f, nullptr};
}

Operand In(float value)
{
    return {Operand::Float, 0, value, nullptr};
}

Operand Out(u32& value)
{
    return {Operand::OutInteger, 0, 0.0f, &value};
}

Operand Out(float& value)
{
    return {Operand::OutFloat, 0, 0.0f, &value};
}

namespace
{
struct Window
{
    u32 base;
    u8* host;
    u32 size;
};

std::vector<Window> s_Windows;

u8* Host(u64 address)
{
    if (address >> 32 == 0)
    {
        for (const Window& w : s_Windows)
            if (address >= w.base && address < static_cast<u64>(w.base) + w.size)
                return w.host + (address - w.base);
    }
    return reinterpret_cast<u8*>(address);
}

struct Gpr
{
    u64 lo;
    u64 hi;
};

[[noreturn]] void Die(const std::string& what)
{
    std::fprintf(stderr, "reference asm: %s\n", what.c_str());
    std::abort();
}

std::string Trim(const std::string& text)
{
    size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = text.find_last_not_of(" \t");
    return text.substr(start, end - start + 1);
}

std::vector<std::string> SplitOperands(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;
    for (char c : text)
    {
        if (c == ',')
        {
            out.push_back(Trim(current));
            current.clear();
        }
        else
            current += c;
    }
    if (!Trim(current).empty())
        out.push_back(Trim(current));
    return out;
}

struct Machine
{
    Gpr gpr[32] = {};
    std::map<int, Gpr> placeholders;
    std::map<int, float> fprs;

    Gpr& Reg(const std::string& name)
    {
        if (name[0] == '%')
            return placeholders[std::atoi(name.c_str() + 1)];
        if (name[0] != '$')
            Die("register " + name);
        std::string n = name.substr(1);
        static const std::map<std::string, int> names = {{"zero", 0}, {"a4", 8}, {"a5", 9}, {"a6", 10}, {"a7", 11}, {"t0", 12},
                                                         {"t1", 13}, {"t2", 14}, {"t3", 15}, {"t8", 24}, {"t9", 25}};
        int index;
        auto found = names.find(n);
        if (found != names.end())
            index = found->second;
        else if (std::isdigit(static_cast<unsigned char>(n[0])))
            index = std::atoi(n.c_str());
        else
            Die("register " + name);
        return gpr[index];
    }

    u64 Value(const std::string& name)
    {
        if (name == "$0")
            return 0;
        return Reg(name).lo;
    }

    void Set(const std::string& name, Gpr value)
    {
        if (name == "$0")
            return;
        Reg(name) = value;
    }

    void SetLo(const std::string& name, u64 value)
    {
        if (name == "$0")
            return;
        Reg(name).lo = value;
    }

    // off(reg)
    u64 Address(const std::string& text)
    {
        size_t open = text.find('(');
        size_t close = text.find(')');
        s64 offset = std::strtoll(text.substr(0, open).c_str(), nullptr, 0);
        return Value(text.substr(open + 1, close - open - 1)) + static_cast<u64>(offset);
    }
};

// $vfN, $vfNx
u32 VfNumber(const std::string& text, int* field)
{
    if (text.compare(0, 3, "$vf") != 0)
        Die("vf operand " + text);
    size_t end = 3;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
        end++;
    u32 number = static_cast<u32>(std::atoi(text.substr(3, end - 3).c_str()));
    if (field != nullptr)
    {
        *field = -1;
        if (end < text.size())
        {
            *field = static_cast<int>(std::string("xyzw").find(text[end]));
            if (*field < 0)
                Die("field " + text);
        }
    }
    return number;
}

u32 DestBits(const std::string& text)
{
    u32 dest = 0;
    for (char c : text)
    {
        if (c == 'x') dest |= 8;
        else if (c == 'y') dest |= 4;
        else if (c == 'z') dest |= 2;
        else if (c == 'w') dest |= 1;
        else Die("dest " + text);
    }
    return dest;
}

u32 Special(u32 code)
{
    return ((code >> 2) << 6) | 0x3C | (code & 3);
}

// A COP2 macro instruction's word (upper format; MOVE, MR32, DIV, SQRT, RSQRT, WAITQ lower format). Whether it's a lower one, and
// whether it writes Q
u32 Assemble(const std::string& mnemonic, const std::vector<std::string>& operands, bool* lower, bool* writesQ)
{
    *lower = false;
    *writesQ = false;
    std::string name = mnemonic;
    u32 dest = 0;
    size_t dot = mnemonic.find('.');
    if (dot != std::string::npos)
    {
        name = mnemonic.substr(0, dot);
        dest = DestBits(mnemonic.substr(dot + 1));
    }
    auto lowerWord = [&](u32 kind, u32 sub, u32 fs, u32 fsf, u32 ft, u32 ftf, u32 d) {
        *lower = true;
        return (0x40u << 25) | (ftf << 23) | (fsf << 21) | (d << 21) | (ft << 16) | (fs << 11) | (sub << 6) | 0x3C | kind;
    };
    if (name == "nop")
        return Special(0x2F);
    if (name == "waitq")
        return lowerWord(3, 0x0E, 0, 0, 0, 0, 0);
    if (name == "move" || name == "mr32")
        return lowerWord(name == "move" ? 0 : 1, 0x0C, VfNumber(operands[1], nullptr), 0, VfNumber(operands[0], nullptr), 0, dest);
    if (name == "div" || name == "rsqrt")
    {
        int fsf, ftf;
        u32 fs = VfNumber(operands[1], &fsf);
        u32 ft = VfNumber(operands[2], &ftf);
        *writesQ = true;
        return lowerWord(name == "div" ? 0 : 2, 0x0E, fs, static_cast<u32>(fsf), ft, static_cast<u32>(ftf), 0);
    }
    if (name == "sqrt")
    {
        int ftf;
        u32 ft = VfNumber(operands[1], &ftf);
        *writesQ = true;
        return lowerWord(1, 0x0E, 0, 0, ft, static_cast<u32>(ftf), 0);
    }
    if (name == "opmula")
        return Special(0x2E) | (VfNumber(operands[2], nullptr) << 16) | (VfNumber(operands[1], nullptr) << 11) | (dest << 21);
    if (name == "opmsub")
        return 0x2E | (VfNumber(operands[2], nullptr) << 16) | (VfNumber(operands[1], nullptr) << 11) |
               (VfNumber(operands[0], nullptr) << 6) | (dest << 21);
    if (name == "abs")
        return Special(0x1D) | (VfNumber(operands[0], nullptr) << 16) | (VfNumber(operands[1], nullptr) << 11) | (dest << 21);
    if (name == "clipw")
        return Special(0x1F) | (VfNumber(operands[1], nullptr) << 16) | (VfNumber(operands[0], nullptr) << 11) | (dest << 21);

    bool acc = operands[0] == "$ACC";
    const std::string& last = operands.back();
    enum { Vector, Broadcast, ImmI, ImmQ } kind = Vector;
    int bc = -1;
    if (last == "$Q")
        kind = ImmQ;
    else if (last == "$I")
        kind = ImmI;
    else
    {
        VfNumber(last, &bc);
        if (bc >= 0)
            kind = Broadcast;
    }
    std::string base = name;
    if (kind != Vector)
        base.pop_back();
    if (acc)
    {
        if (base.back() != 'a')
            Die("acc op " + mnemonic);
        base.pop_back();
    }
    static const std::map<std::string, u32> groups = {{"add", 0}, {"sub", 1}, {"madd", 2}, {"msub", 3},
                                                      {"max", 4}, {"mini", 5}, {"mul", 6}};
    if (!groups.count(base))
        Die("op " + mnemonic);
    u32 group = groups.at(base);
    u32 fs = VfNumber(operands[1], nullptr);
    u32 ft = (kind == Vector || kind == Broadcast) ? VfNumber(operands[2], nullptr) : 0;
    u32 fd = acc ? 0 : VfNumber(operands[0], nullptr);
    u32 fields = (dest << 21) | (ft << 16) | (fs << 11);
    if (!acc)
    {
        fields |= fd << 6;
        if (kind == Broadcast)
            return fields | (group * 4 + static_cast<u32>(bc));
        if (kind == Vector)
        {
            static const std::map<std::string, u32> ops = {{"add", 0x28}, {"madd", 0x29}, {"mul", 0x2A}, {"max", 0x2B},
                                                           {"sub", 0x2C}, {"msub", 0x2D}, {"mini", 0x2F}};
            return fields | ops.at(base);
        }
        static const std::map<std::string, u32> imm = {{"mulq", 0x1C}, {"maxi", 0x1D}, {"muli", 0x1E}, {"minii", 0x1F},
                                                       {"addq", 0x20}, {"maddq", 0x21}, {"addi", 0x22}, {"maddi", 0x23},
                                                       {"subq", 0x24}, {"msubq", 0x25}, {"subi", 0x26}, {"msubi", 0x27}};
        return fields | imm.at(base + (kind == ImmQ ? "q" : "i"));
    }
    if (kind == Broadcast)
        return fields | Special(group * 4 + static_cast<u32>(bc));
    if (kind == Vector)
    {
        static const std::map<std::string, u32> ops = {{"add", 0x28}, {"madd", 0x29}, {"mul", 0x2A}, {"sub", 0x2C}, {"msub", 0x2D}};
        return fields | Special(ops.at(base));
    }
    static const std::map<std::string, u32> imm = {{"mulq", 0x1C}, {"muli", 0x1E}, {"addq", 0x20}, {"maddq", 0x21},
                                                   {"addi", 0x22}, {"maddi", 0x23}, {"subq", 0x24}, {"msubq", 0x25},
                                                   {"subi", 0x26}, {"msubi", 0x27}};
    return fields | Special(imm.at(base + (kind == ImmQ ? "q" : "i")));
}

u32 ViNumber(const std::string& text)
{
    if (text.compare(0, 3, "$vi") != 0)
        Die("vi operand " + text);
    return static_cast<u32>(std::atoi(text.c_str() + 3));
}

struct Instruction
{
    std::string mnemonic;
    std::vector<std::string> operands;
};

u32 Word(const Gpr& gpr, int index)
{
    return static_cast<u32>((index < 2 ? gpr.lo : gpr.hi) >> ((index & 1) * 32));
}

Gpr Words(u32 a, u32 b, u32 c, u32 d)
{
    return {static_cast<u64>(a) | (static_cast<u64>(b) << 32), static_cast<u64>(c) | (static_cast<u64>(d) << 32)};
}
}

void MapLow(u32 base, void* host, u32 size)
{
    s_Windows.push_back({base, static_cast<u8*>(host), size});
}

void UnmapLow()
{
    s_Windows.clear();
}

void Asm(const char* text, std::vector<Operand> operands)
{
    Vu& vu = TheVu();
    Machine m;
    for (size_t n = 0; n < operands.size(); n++)
    {
        if (operands[n].kind == Operand::Integer)
            m.placeholders[static_cast<int>(n)] = {operands[n].value, 0};
        else if (operands[n].kind == Operand::Float)
            m.fprs[static_cast<int>(n)] = operands[n].floatValue;
    }

    // The instructions and their numeric labels
    std::vector<Instruction> program;
    std::map<int, size_t> labels;
    std::string all(text);
    size_t start = 0;
    while (start <= all.size())
    {
        size_t end = all.find('\n', start);
        if (end == std::string::npos)
            end = all.size();
        std::string line = Trim(all.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line[0] == '.')
            continue;
        if (line.back() == ':')
        {
            labels[std::atoi(line.c_str())] = program.size();
            continue;
        }
        size_t space = line.find_first_of(" \t");
        Instruction instruction;
        instruction.mnemonic = line.substr(0, space);
        if (space != std::string::npos)
            instruction.operands = SplitOperands(line.substr(space + 1));
        program.push_back(instruction);
    }

    size_t pc = 0;
    long branchTo = -1;
    int delay = 0;
    u32 steps = 0;
    auto fpr = [&](const std::string& name) -> float& { return m.fprs[std::atoi(name.c_str() + 1)]; };
    while (pc < program.size())
    {
        if (++steps > 10000000)
            Die("runaway asm");
        const Instruction& in = program[pc];
        const std::string& op = in.mnemonic;
        const auto& a = in.operands;
        size_t next = pc + 1;
        if (op == "nop")
        {
        }
        else if (op == "lqc2" || op == "sqc2")
        {
            u32 vf = VfNumber(a[0], nullptr);
            auto* address = Host(m.Address(a[1]));
            if (op == "lqc2")
            {
                if (vf != 0)
                    std::memcpy(vu.vf[vf], address, 16);
            }
            else
                std::memcpy(address, vu.vf[vf], 16);
        }
        else if (op == "lq" || op == "sq")
        {
            // The EE's LQ and SQ drop the address's low 4 bits
            auto* address = Host(m.Address(a[1]) & ~static_cast<u64>(15));
            if (op == "lq")
            {
                Gpr value;
                std::memcpy(&value, address, 16);
                m.Set(a[0], value);
            }
            else
            {
                Gpr value = a[0] == "$0" ? Gpr{} : m.Reg(a[0]);
                std::memcpy(address, &value, 16);
            }
        }
        else if (op == "lw")
        {
            s32 value;
            std::memcpy(&value, Host(m.Address(a[1])), 4);
            m.SetLo(a[0], static_cast<u64>(static_cast<s64>(value)));
        }
        else if (op == "sw")
        {
            u32 value = static_cast<u32>(m.Value(a[0]));
            std::memcpy(Host(m.Address(a[1])), &value, 4);
        }
        else if (op == "lui")
        {
            u32 imm = static_cast<u32>(std::strtoul(a[1].c_str(), nullptr, 0));
            m.SetLo(a[0], static_cast<u64>(static_cast<s64>(static_cast<s32>(imm << 16))));
        }
        else if (op == "addi" || op == "addiu")
        {
            // 64 bit natively: the registers hold the host's addresses
            m.SetLo(a[0], m.Value(a[1]) + static_cast<u64>(std::strtoll(a[2].c_str(), nullptr, 0)));
        }
        else if (op == "daddu")
            m.SetLo(a[0], m.Value(a[1]) + m.Value(a[2]));
        else if (op == "bnez")
        {
            if (m.Value(a[0]) != 0)
            {
                std::string label = a[1];
                branchTo = static_cast<long>(labels.at(std::atoi(label.c_str())));
            }
            delay = 2;
        }
        else if (op == "qmfc2.ni" || op == "qmfc2.i")
        {
            u32* w = vu.vf[VfNumber(a[1], nullptr)];
            m.Set(a[0], Words(w[0], w[1], w[2], w[3]));
        }
        else if (op == "qmtc2.ni" || op == "qmtc2.i")
        {
            u32 vf = VfNumber(a[1], nullptr);
            Gpr g = a[0] == "$0" ? Gpr{} : m.Reg(a[0]);
            if (vf != 0)
                for (int n = 0; n < 4; n++)
                    vu.vf[vf][n] = Word(g, n);
        }
        else if (op == "ctc2.ni" || op == "ctc2.i")
        {
            u32 reg = ViNumber(a[1]);
            u32 value = static_cast<u32>(m.Value(a[0]));
            if (reg == 21)
                vu.i = value;
            else if (reg > 0 && reg < 16)
                vu.vi[reg] = static_cast<u16>(value);
            else
                Die("ctc2 to " + a[1]);
        }
        else if (op == "cfc2.ni" || op == "cfc2.i")
        {
            u32 reg = ViNumber(a[1]);
            u32 value;
            if (reg < 16)
                value = vu.vi[reg];
            else if (reg == 18)
                value = vu.clipflag;
            else
                Die("cfc2 from " + a[1]);
            m.SetLo(a[0], value);
        }
        else if (op == "pextlw" || op == "pextuw" || op == "pcpyld" || op == "pcpyud")
        {
            Gpr rs = m.Reg(a[1]);
            Gpr rt = m.Reg(a[2]);
            Gpr rd;
            if (op == "pextlw")
                rd = Words(Word(rt, 0), Word(rs, 0), Word(rt, 1), Word(rs, 1));
            else if (op == "pextuw")
                rd = Words(Word(rt, 2), Word(rs, 2), Word(rt, 3), Word(rs, 3));
            else if (op == "pcpyld")
                rd = {rt.lo, rs.lo};
            else
                rd = {rs.hi, rt.hi};
            m.Set(a[0], rd);
        }
        else if (op == "max.s" || op == "min.s" || op == "rsqrt.s")
        {
            u32 s, t;
            float fs = fpr(a[1]);
            float ft = fpr(a[2]);
            std::memcpy(&s, &fs, 4);
            std::memcpy(&t, &ft, 4);
            u32 d = op == "max.s" ? FpuMax(s, t) : op == "min.s" ? FpuMin(s, t) : FpuRsqrt(s, t);
            float result;
            std::memcpy(&result, &d, 4);
            fpr(a[0]) = result;
        }
        else if (op == "vcallms")
            RunMicroprogram(static_cast<u32>(std::strtoul(a[0].c_str(), nullptr, 0)));
        else if (op[0] == 'v')
        {
            bool lower;
            bool writesQ;
            u32 word = Assemble(op.substr(1), a, &lower, &writesQ);
            if (lower)
                MacroLower(word, writesQ);
            else
                MacroUpper(word);
        }
        else
            Die("instruction " + op);

        // .set noreorder: a branch's delay slot runs before it's taken
        if (delay > 0 && --delay == 0 && branchTo >= 0)
        {
            next = static_cast<size_t>(branchTo);
            branchTo = -1;
        }
        else if (delay == 0)
            branchTo = -1;
        pc = next;
    }

    for (size_t n = 0; n < operands.size(); n++)
    {
        if (operands[n].kind == Operand::OutInteger)
            *static_cast<u32*>(operands[n].out) = static_cast<u32>(m.placeholders[static_cast<int>(n)].lo);
        else if (operands[n].kind == Operand::OutFloat)
            *static_cast<float*>(operands[n].out) = m.fprs[static_cast<int>(n)];
    }
}
}
