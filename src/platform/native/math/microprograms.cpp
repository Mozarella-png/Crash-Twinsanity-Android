// Made by native/math-tests/tools/vu0translate.py from the retail executable's VU0 microcode (assets/vutext.textbin.bin):
// do not edit. Each of the maths' VU0 microprograms an instruction pair at a time on the native VU0, with the pipelines'
// timing as PCSX2's VU0 interpreter runs it worked out ahead of time (see the tool and native/MATH.md): a read of Q or of
// the MAC flags reads the variable holding the value it gets, a lower instruction reading its upper instruction's
// register reads its old value from Vu0::Temporary, branches go to their targets after their delay slots.

#include "microprograms.h"

namespace NativeMath
{
namespace
{
// The standard set's program at 0x0F0 (Platform::Math::SinCos)
void MicroprogramStd0F0(Vu0& vu)
{
    // 0F0: 0154A7EB 8000033C  max.xz vf31, vf20, vf20                      nop
    vu.Max(XZ, 31, 20, 20);
    // 0F8: 003F07DA 8000033C  mulz.w vf31, vf00, vf31z                     nop
    vu.MulBc(W, 31, 0, 31, Fz);
    // 100: 809F07C0 3FC90FD8  addx[i].y vf31, vf00, vf31x                  loi 0x3FC90FD8 (1.570796012878418)
    vu.AddBc(Y, 31, 0, 31, Fx);
    vu.i = 0x3FC90FD8;
    // 108: 8140FFE6 BE22F987  subi[i].xz vf31, vf31, i                     loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(XZ, 31, 31);
    vu.i = 0xBE22F987;
    // 110: 01FFF9FD 8000033C  abs.xyzw vf31, vf31                          nop
    vu.Abs(XYZW, 31, 31);
    // 118: 01E00053 8000033C  maxw.xyzw vf01, vf00, vf00w                  nop
    vu.MaxBc(XYZW, 1, 0, 0, Fw);
    // 120: 81E0F9FE 4B400000  mulai[i].xyzw ACC, vf31, i                   loi 0x4B400000 (12582912.0)
    vu.MulaI(XYZW, 31);
    vu.i = 0x4B400000;
    // 128: 01E00A7F 8000033C  msubai.xyzw ACC, vf01, i                     nop
    vu.MsubaI(XYZW, 1);
    // 130: 81E00A3F BE22F987  maddai[i].xyzw ACC, vf01, i                  loi 0xBE22F987 (-0.1591549962759018)
    vu.MaddaI(XYZW, 1);
    vu.i = 0xBE22F987;
    // 138: 81E0FA7F 3F000000  msubai[i].xyzw ACC, vf31, i                  loi 0x3F000000 (0.5)
    vu.MsubaI(XYZW, 31);
    vu.i = 0x3F000000;
    // 140: 81E00FE7 3E800000  msubi[i].xyzw vf31, vf01, i                  loi 0x3E800000 (0.25)
    vu.MsubI(XYZW, 31, 1);
    vu.i = 0x3E800000;
    // 148: 01FFF9FD 8000033C  abs.xyzw vf31, vf31                          nop
    vu.Abs(XYZW, 31, 31);
    // 150: 01E0FFE6 8000033C  subi.xyzw vf31, vf31, i                      nop
    vu.SubI(XYZW, 31, 31);
    // 158: 81FFF8EA C2992661  mul[i].xyzw vf03, vf31, vf31                 loi 0xC2992661 (-76.57495880126953)
    vu.Mul(XYZW, 3, 31, 31);
    vu.i = 0xC2992661;
    // 160: 81E0F95E C2255DE0  muli[i].xyzw vf05, vf31, i                   loi 0xC2255DE0 (-41.3416748046875)
    vu.MulI(XYZW, 5, 31);
    vu.i = 0xC2255DE0;
    // 168: 81E0F91E 42A33457  muli[i].xyzw vf04, vf31, i                   loi 0x42A33457 (81.60222625732422)
    vu.MulI(XYZW, 4, 31);
    vu.i = 0x42A33457;
    // 170: 01E0F85E 8000033C  muli.xyzw vf01, vf31, i                      nop
    vu.MulI(XYZW, 1, 31);
    // 178: 01E318AA 8000033C  mul.xyzw vf02, vf03, vf03                    nop
    vu.Mul(XYZW, 2, 3, 3);
    // 180: 01E3296A 8000033C  mul.xyzw vf05, vf05, vf03                    nop
    vu.Mul(XYZW, 5, 5, 3);
    // 188: 81E322BE 421ED7B7  mula[i].xyzw ACC, vf04, vf03                 loi 0x421ED7B7 (39.71065902709961)
    vu.Mula(XYZW, 4, 3);
    vu.i = 0x421ED7B7;
    // 190: 01E0F8DE 8000033C  muli.xyzw vf03, vf31, i                      nop
    vu.MulI(XYZW, 3, 31);
    // 198: 01E2112A 8000033C  mul.xyzw vf04, vf02, vf02                    nop
    vu.Mul(XYZW, 4, 2, 2);
    // 1A0: 01E22ABD 8000033C  madda.xyzw ACC, vf05, vf02                   nop
    vu.Madda(XYZW, 5, 2);
    // 1A8: 81E20ABD 40C90FDA  madda[i].xyzw ACC, vf01, vf02                loi 0x40C90FDA (6.283185005187988)
    vu.Madda(XYZW, 1, 2);
    vu.i = 0x40C90FDA;
    // 1B0: 41E0FA3F 8000033C  maddai[e].xyzw ACC, vf31, i                  nop
    vu.MaddaI(XYZW, 31);
    // 1B8: 01E41FE9 8000033C  madd.xyzw vf31, vf03, vf04                   nop
    vu.Madd(XYZW, 31, 3, 4);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd0F0Ranges[] = {{0x1E, 0x38}};

// The standard set's program at 0x1C0 (Platform::Math::SlerpRotations)
void MicroprogramStd1C0(Vu0& vu)
{
    u32 mac1D8;
    bool taken200;
    u32 mac230;
    bool taken240;
    u32 q4F8;

    // 1C0: 01E1FA6A 10010080  mul.xyzw vf09, vf31, vf01                    iaddiu vi01, vi00, 0x80
    vu.Mul(XYZW, 9, 31, 1);
    vu.Iaddiu(1, 0, 0x80);
    // 1C8: 81094A41 BF800000  addy[i].x vf09, vf09, vf09y                  loi 0xBF800000 (-1.0)
    vu.AddBc(X, 9, 9, 9, Fy);
    vu.i = 0xBF800000;
    // 1D0: 01094A42 8000033C  addz.x vf09, vf09, vf09z                     nop
    vu.AddBc(X, 9, 9, 9, Fz);
    // 1D8: 01094A43 8000033C  addw.x vf09, vf09, vf09w                     nop
    vu.AddBc(X, 9, 9, 9, Fw);
    mac1D8 = vu.mac;
    // 1E0: 000002FF 8000033C  nop                                          nop
    // 1E8: 000002FF 8000033C  nop                                          nop
    // 1F0: 000002FF 8000033C  nop                                          nop
    // 1F8: 01E10AAB 34010800  max.xyzw vf10, vf01, vf01                    fmand vi01, vi01
    vu.Max(XYZW, 10, 1, 1);
    vu.Fmand(1, 1, mac1D8);
    // 200: 000002FF 50010003  nop                                          ibeq vi01, vi00, 0x220
    taken200 = static_cast<s16>(vu.vi[1]) == static_cast<s16>(vu.vi[0]);
    // 208: 000002FF 8000033C  nop                                          nop
    if (taken200)
    {
        goto L220;
    }
    // 210: 01004A5E 8000033C  muli.x vf09, vf09, i                         nop
    vu.MulI(X, 9, 9);
    // 218: 01E0529E 8000033C  muli.xyzw vf10, vf10, i                      nop
    vu.MulI(XYZW, 10, 10);
L220:
    // 220: 000002FF 8000033C  nop                                          nop
    // 228: 800002FF 3F7FBE77  nop[i]                                       loi 0x3F7FBE77 (0.9990000128746033)
    vu.i = 0x3F7FBE77;
    // 230: 01004AE6 10010080  subi.x vf11, vf09, i                         iaddiu vi01, vi00, 0x80
    vu.SubI(X, 11, 9);
    vu.Iaddiu(1, 0, 0x80);
    mac230 = vu.mac;
    // 238: 010059FD 34010800  abs.x vf00, vf11                             fmand vi01, vi01
    vu.Abs(X, 0, 11);
    vu.Fmand(1, 1, mac230);
    // 240: 000002FF 52010006  nop                                          ibne vi01, vi00, 0x278
    taken240 = static_cast<s16>(vu.vi[1]) != static_cast<s16>(vu.vi[0]);
    // 248: 000002FF 8000033C  nop                                          nop
    if (taken240)
    {
        goto L278;
    }
    // 250: 00400743 810BEB3C  addw.z vf29, vf00, vf00w                     move.x vf11, vf29
    vu.vf[Vu0::Temporary] = vu.vf[29]; // the lower instruction reads it as it was
    vu.AddBc(Z, 29, 0, 0, Fw);
    vu.Move(X, 11, Vu0::Temporary);
    // 258: 000002FF 8000033C  nop                                          nop
    // 260: 000002FF 8000033C  nop                                          nop
    // 268: 000002FF 40000056  nop                                          b 0x520
    // 270: 005DEAC4 8000033C  subx.z vf11, vf29, vf29x                     nop
    vu.SubBc(Z, 11, 29, 29, Fx);
    goto L520;
L278:
    // 278: 80400243 3FC90FDB  addw[i].z vf09, vf00, vf00w                  loi 0x3FC90FDB (1.5707963705062866)
    vu.AddBc(Z, 9, 0, 0, Fw);
    vu.i = 0x3FC90FDB;
    // 280: 81004ADE 3FC90FDB  muli[i].x vf11, vf09, i                      loi 0x3FC90FDB (1.5707963705062866)
    vu.MulI(X, 11, 9);
    vu.i = 0x3FC90FDB;
    // 288: 81005AE6 BE22F987  subi[i].x vf11, vf11, i                      loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(X, 11, 11);
    vu.i = 0xBE22F987;
    // 290: 010C59FD 8000033C  abs.x vf12, vf11                             nop
    vu.Abs(X, 12, 11);
    // 298: 010002D3 8000033C  maxw.x vf11, vf00, vf00w                     nop
    vu.MaxBc(X, 11, 0, 0, Fw);
    // 2A0: 810061FE 4B400000  mulai[i].x ACC, vf12, i                      loi 0x4B400000 (12582912.0)
    vu.MulaI(X, 12);
    vu.i = 0x4B400000;
    // 2A8: 01005A7F 8000033C  msubai.x ACC, vf11, i                        nop
    vu.MsubaI(X, 11);
    // 2B0: 81005A3F BE22F987  maddai[i].x ACC, vf11, i                     loi 0xBE22F987 (-0.1591549962759018)
    vu.MaddaI(X, 11);
    vu.i = 0xBE22F987;
    // 2B8: 8100627F 3F000000  msubai[i].x ACC, vf12, i                     loi 0x3F000000 (0.5)
    vu.MsubaI(X, 12);
    vu.i = 0x3F000000;
    // 2C0: 81005AE7 3E800000  msubi[i].x vf11, vf11, i                     loi 0x3E800000 (0.25)
    vu.MsubI(X, 11, 11);
    vu.i = 0x3E800000;
    // 2C8: 010B59FD 8000033C  abs.x vf11, vf11                             nop
    vu.Abs(X, 11, 11);
    // 2D0: 01005AE6 8000033C  subi.x vf11, vf11, i                         nop
    vu.SubI(X, 11, 11);
    // 2D8: 810B5BEA C2992661  mul[i].x vf15, vf11, vf11                    loi 0xC2992661 (-76.57495880126953)
    vu.Mul(X, 15, 11, 11);
    vu.i = 0xC2992661;
    // 2E0: 81005C1E C2255DE0  muli[i].x vf16, vf11, i                      loi 0xC2255DE0 (-41.3416748046875)
    vu.MulI(X, 16, 11);
    vu.i = 0xC2255DE0;
    // 2E8: 81005C9E 42A33457  muli[i].x vf18, vf11, i                      loi 0x42A33457 (81.60222625732422)
    vu.MulI(X, 18, 11);
    vu.i = 0x42A33457;
    // 2F0: 01005B1E 8000033C  muli.x vf12, vf11, i                         nop
    vu.MulI(X, 12, 11);
    // 2F8: 010F7B6A 8000033C  mul.x vf13, vf15, vf15                       nop
    vu.Mul(X, 13, 15, 15);
    // 300: 010F842A 8000033C  mul.x vf16, vf16, vf15                       nop
    vu.Mul(X, 16, 16, 15);
    // 308: 810F92BE 421ED7B7  mula[i].x ACC, vf18, vf15                    loi 0x421ED7B7 (39.71065902709961)
    vu.Mula(X, 18, 15);
    vu.i = 0x421ED7B7;
    // 310: 01005BDE 8000033C  muli.x vf15, vf11, i                         nop
    vu.MulI(X, 15, 11);
    // 318: 010D6CAA 8000033C  mul.x vf18, vf13, vf13                       nop
    vu.Mul(X, 18, 13, 13);
    // 320: 010D82BD 8000033C  madda.x ACC, vf16, vf13                      nop
    vu.Madda(X, 16, 13);
    // 328: 810D62BD 40C90FDA  madda[i].x ACC, vf12, vf13                   loi 0x40C90FDA (6.283185005187988)
    vu.Madda(X, 12, 13);
    vu.i = 0x40C90FDA;
    // 330: 01005A3F 8000033C  maddai.x ACC, vf11, i                        nop
    vu.MaddaI(X, 11);
    // 338: 81127B29 BFC90FDB  madd[i].x vf12, vf15, vf18                   loi 0xBFC90FDB (-1.5707963705062866)
    vu.Madd(X, 12, 15, 18);
    vu.i = 0xBFC90FDB;
    // 340: 01004ADE 8000033C  muli.x vf11, vf09, i                         nop
    vu.MulI(X, 11, 9);
    // 348: 0100625E 8000033C  muli.x vf09, vf12, i                         nop
    vu.MulI(X, 9, 12);
    // 350: 01005AE6 8000033C  subi.x vf11, vf11, i                         nop
    vu.SubI(X, 11, 11);
    // 358: 01004A66 8000033C  subi.x vf09, vf09, i                         nop
    vu.SubI(X, 9, 9);
    // 360: 010B5AE8 8000033C  add.x vf11, vf11, vf11                       nop
    vu.Add(X, 11, 11, 11);
    // 368: 01095A6C 8000033C  sub.x vf09, vf11, vf09                       nop
    vu.Sub(X, 9, 11, 9);
    // 370: 000002FF 8000033C  nop                                          nop
    // 378: 000002FF 8000033C  nop                                          nop
    // 380: 805D4F44 3FC90FD8  subx[i].z vf29, vf09, vf29x                  loi 0x3FC90FD8 (1.570796012878418)
    vu.SubBc(Z, 29, 9, 29, Fx);
    vu.i = 0x3FC90FD8;
    // 388: 81004AE6 3FC90FD8  subi[i].x vf11, vf09, i                      loi 0x3FC90FD8 (1.570796012878418)
    vu.SubI(X, 11, 9);
    vu.i = 0x3FC90FD8;
    // 390: 0149EA58 8000033C  mulx.xz vf09, vf29, vf09x                    nop
    vu.MulBc(XZ, 9, 29, 9, Fx);
    // 398: 011D59FD 8000033C  abs.x vf29, vf11                             nop
    vu.Abs(X, 29, 11);
    // 3A0: 01000313 8000033C  maxw.x vf12, vf00, vf00w                     nop
    vu.MaxBc(X, 12, 0, 0, Fw);
    // 3A8: 81404AE6 BE22F987  subi[i].xz vf11, vf09, i                     loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(XZ, 11, 9);
    vu.i = 0xBE22F987;
    // 3B0: 8100E9FE 4B400000  mulai[i].x ACC, vf29, i                      loi 0x4B400000 (12582912.0)
    vu.MulaI(X, 29);
    vu.i = 0x4B400000;
    // 3B8: 0100627F 8000033C  msubai.x ACC, vf12, i                        nop
    vu.MsubaI(X, 12);
    // 3C0: 8100623F BE22F987  maddai[i].x ACC, vf12, i                     loi 0xBE22F987 (-0.1591549962759018)
    vu.MaddaI(X, 12);
    vu.i = 0xBE22F987;
    // 3C8: 8100EA7F 3F000000  msubai[i].x ACC, vf29, i                     loi 0x3F000000 (0.5)
    vu.MsubaI(X, 29);
    vu.i = 0x3F000000;
    // 3D0: 01006767 8000033C  msubi.x vf29, vf12, i                        nop
    vu.MsubI(X, 29, 12);
    // 3D8: 014B59FD 8000033C  abs.xz vf11, vf11                            nop
    vu.Abs(XZ, 11, 11);
    // 3E0: 01400253 8000033C  maxw.xz vf09, vf00, vf00w                    nop
    vu.MaxBc(XZ, 9, 0, 0, Fw);
    // 3E8: 811DE9FD BE22F987  abs[i].x vf29, vf29                          loi 0xBE22F987 (-0.1591549962759018)
    vu.Abs(X, 29, 29);
    vu.i = 0xBE22F987;
    // 3F0: 814059FE 4B400000  mulai[i].xz ACC, vf11, i                     loi 0x4B400000 (12582912.0)
    vu.MulaI(XZ, 11);
    vu.i = 0x4B400000;
    // 3F8: 01404A7F 8000033C  msubai.xz ACC, vf09, i                       nop
    vu.MsubaI(XZ, 9);
    // 400: 81404A3F 3E800000  maddai[i].xz ACC, vf09, i                    loi 0x3E800000 (0.25)
    vu.MaddaI(XZ, 9);
    vu.i = 0x3E800000;
    // 408: 8100EF66 BE22F987  subi[i].x vf29, vf29, i                      loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(X, 29, 29);
    vu.i = 0xBE22F987;
    // 410: 81405A7F 3F000000  msubai[i].xz ACC, vf11, i                    loi 0x3F000000 (0.5)
    vu.MsubaI(XZ, 11);
    vu.i = 0x3F000000;
    // 418: 01404A67 8000033C  msubi.xz vf09, vf09, i                       nop
    vu.MsubI(XZ, 9, 9);
    // 420: 011DEB2A 8000033C  mul.x vf12, vf29, vf29                       nop
    vu.Mul(X, 12, 29, 29);
    // 428: 014949FD 8000033C  abs.xz vf09, vf09                            nop
    vu.Abs(XZ, 9, 9);
    // 430: 000002FF 8000033C  nop                                          nop
    // 438: 000002FF 8000033C  nop                                          nop
    // 440: 810C636A 3E800000  mul[i].x vf13, vf12, vf12                    loi 0x3E800000 (0.25)
    vu.Mul(X, 13, 12, 12);
    vu.i = 0x3E800000;
    // 448: 81404A66 C2992661  subi[i].xz vf09, vf09, i                     loi 0xC2992661 (-76.57495880126953)
    vu.SubI(XZ, 9, 9);
    vu.i = 0xC2992661;
    // 450: 8100EC9E C2255DE0  muli[i].x vf18, vf29, i                      loi 0xC2255DE0 (-41.3416748046875)
    vu.MulI(X, 18, 29);
    vu.i = 0xC2255DE0;
    // 458: 8100EC1E 42A33457  muli[i].x vf16, vf29, i                      loi 0x42A33457 (81.60222625732422)
    vu.MulI(X, 16, 29);
    vu.i = 0x42A33457;
    // 460: 0100EBDE 8000033C  muli.x vf15, vf29, i                         nop
    vu.MulI(X, 15, 29);
    // 468: 01494AEA 8000033C  mul.xz vf11, vf09, vf09                      nop
    vu.Mul(XZ, 11, 9, 9);
    // 470: 810C94AA 421ED7B7  mul[i].x vf18, vf18, vf12                    loi 0x421ED7B7 (39.71065902709961)
    vu.Mul(X, 18, 18, 12);
    vu.i = 0x421ED7B7;
    // 478: 0100EB9E 8000033C  muli.x vf14, vf29, i                         nop
    vu.MulI(X, 14, 29);
    // 480: 010C82BE 8000033C  mula.x ACC, vf16, vf12                       nop
    vu.Mula(X, 16, 12);
    // 488: 014B5B2A 8000033C  mul.xz vf12, vf11, vf11                      nop
    vu.Mul(XZ, 12, 11, 11);
    // 490: 810D92BD C2992661  madda[i].x ACC, vf18, vf13                   loi 0xC2992661 (-76.57495880126953)
    vu.Madda(X, 18, 13);
    vu.i = 0xC2992661;
    // 498: 81404C9E C2255DE0  muli[i].xz vf18, vf09, i                     loi 0xC2255DE0 (-41.3416748046875)
    vu.MulI(XZ, 18, 9);
    vu.i = 0xC2255DE0;
    // 4A0: 01404C1E 8000033C  muli.xz vf16, vf09, i                        nop
    vu.MulI(XZ, 16, 9);
    // 4A8: 010D6C6A 8000033C  mul.x vf17, vf13, vf13                       nop
    vu.Mul(X, 17, 13, 13);
    // 4B0: 010D7ABD 8000033C  madda.x ACC, vf15, vf13                      nop
    vu.Madda(X, 15, 13);
    // 4B8: 814B94EA 42A33457  mul[i].xz vf19, vf18, vf11                   loi 0x42A33457 (81.60222625732422)
    vu.Mul(XZ, 19, 18, 11);
    vu.i = 0x42A33457;
    // 4C0: 81404C9E 421ED7B7  muli[i].xz vf18, vf09, i                     loi 0x421ED7B7 (39.71065902709961)
    vu.MulI(XZ, 18, 9);
    vu.i = 0x421ED7B7;
    // 4C8: 81404BDE 40C90FDA  muli[i].xz vf15, vf09, i                     loi 0x40C90FDA (6.283185005187988)
    vu.MulI(XZ, 15, 9);
    vu.i = 0x40C90FDA;
    // 4D0: 0100EA3F 8000033C  maddai.x ACC, vf29, i                        nop
    vu.MaddaI(X, 29);
    // 4D8: 01117769 8000033C  madd.x vf29, vf14, vf17                      nop
    vu.Madd(X, 29, 14, 17);
    // 4E0: 000002FF 8000033C  nop                                          nop
    // 4E8: 014C636A 8000033C  mul.xz vf13, vf12, vf12                      nop
    vu.Mul(XZ, 13, 12, 12);
    // 4F0: 014B82BE 8000033C  mula.xz ACC, vf16, vf11                      nop
    vu.Mula(XZ, 16, 11);
    // 4F8: 014C9ABD 807D03BC  madda.xz ACC, vf19, vf12                     div Q, vf00.w, vf29.x
    vu.Madda(XZ, 19, 12);
    q4F8 = vu.DivValue(0, Fw, 29, Fx);
    // 500: 814C92BD 40C90FDA  madda[i].xz ACC, vf18, vf12                  loi 0x40C90FDA (6.283185005187988)
    vu.Madda(XZ, 18, 12);
    vu.i = 0x40C90FDA;
    // 508: 01404A3F 8000033C  maddai.xz ACC, vf09, i                       nop
    vu.MaddaI(XZ, 9);
    // 510: 014D7A69 8000033C  madd.xz vf09, vf15, vf13                     nop
    vu.Madd(XZ, 9, 15, 13);
    vu.q = q4F8;
    // 518: 01404ADC 800003BF  mulq.xz vf11, vf09, q                        waitq
    vu.MulQ(XZ, 11, 9);
L520:
    // 520: 000002FF 8000033C  nop                                          nop
    // 528: 000002FF 8000033C  nop                                          nop
    // 530: 000002FF 8000033C  nop                                          nop
    // 538: 41EBF9BE 8000033C  mulaz[e].xyzw ACC, vf31, vf11z               nop
    vu.MulaBc(XYZW, 31, 11, Fz);
    // 540: 01EB5048 8000033C  maddx.xyzw vf01, vf10, vf11x                 nop
    vu.MaddBc(XYZW, 1, 10, 11, Fx);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd1C0Ranges[] = {{0x38, 0xA9}};

// The standard set's program at 0x548 (Platform::Math::JointMatrix (a rotation, no scale))
void MicroprogramStd548(Vu0& vu)
{
    u32 q550;
    u32 q588;
    u32 q5C0;

    // 548: 81DFFABE 3FB504F3  mula[i].xyz ACC, vf31, vf31                  loi 0x3FB504F3 (1.4142135381698608)
    vu.Mula(XYZ, 31, 31);
    vu.i = 0x3FB504F3;
    // 550: 01E0FA5E 807D03BC  muli.xyzw vf09, vf31, i                      div Q, vf00.w, vf29.x
    vu.MulI(XYZW, 9, 31);
    q550 = vu.DivValue(0, Fw, 29, Fx);
    // 558: 000002FF 8000033C  nop                                          nop
    // 560: 000002FF 8000033C  nop                                          nop
    // 568: 01DFFAE9 8000033C  madd.xyz vf11, vf31, vf31                    nop
    vu.Madd(XYZ, 11, 31, 31);
    // 570: 01C00343 8000033C  addw.xyz vf13, vf00, vf00w                   nop
    vu.AddBc(XYZ, 13, 0, 0, Fw);
    // 578: 01C94AFE 8000033C  opmula.xyz ACC, vf09, vf09                   nop
    vu.Opmula(9, 9);
    // 580: 01C94FCB 8000033C  maddw.xyz vf31, vf09, vf09w                  nop
    vu.MaddBc(XYZ, 31, 9, 9, Fw);
    vu.q = q550;
    // 588: 01C94A8F 80FD03BC  msubw.xyz vf10, vf09, vf09w                  div Q, vf00.w, vf29.y
    vu.MsubBc(XYZ, 10, 9, 9, Fw);
    q588 = vu.DivValue(0, Fw, 29, Fy);
    // 590: 01C0003F 8000033C  addaw.xyz ACC, vf00, vf00w                   nop
    vu.AddaBc(XYZ, 0, 0, Fw);
    // 598: 00CB68FC 8000033C  msubax.yz ACC, vf13, vf11x                   nop
    vu.MsubaBc(YZ, 13, 11, Fx);
    // 5A0: 010B68FD 8000033C  msubay.x ACC, vf13, vf11y                    nop
    vu.MsubaBc(X, 13, 11, Fy);
    // 5A8: 01000260 8000033C  addq.x vf09, vf00, q                         nop
    vu.AddQ(X, 9, 0);
    // 5B0: 010B6ACE 808C533D  msubz.x vf11, vf13, vf11z                    mr32.y vf12, vf10
    vu.MsubBc(X, 11, 13, 11, Fz);
    vu.Mr32(Y, 12, 10);
    // 5B8: 00E0026C 8000033C  sub.yzw vf09, vf00, vf00                     nop
    vu.Sub(YZW, 9, 0, 0);
    vu.q = q588;
    // 5C0: 008B6B8E 817D03BC  msubz.y vf14, vf13, vf11z                    div Q, vf00.w, vf29.z
    vu.MsubBc(Y, 14, 13, 11, Fz);
    q5C0 = vu.DivValue(0, Fw, 29, Fz);
    // 5C8: 00800760 8000033C  addq.y vf29, vf00, q                         nop
    vu.AddQ(Y, 29, 0);
    // 5D0: 01A0036C 810E633D  sub.xyw vf13, vf00, vf00                     mr32.x vf14, vf12
    vu.Sub(XYW, 13, 0, 0);
    vu.Mr32(X, 14, 12);
    // 5D8: 01E0032C 802BFB3D  sub.xyzw vf12, vf00, vf00                    mr32.w vf11, vf31
    vu.Sub(XYZW, 12, 0, 0);
    vu.Mr32(W, 11, 31);
    // 5E0: 0160076C 8000033C  sub.xzw vf29, vf00, vf00                     nop
    vu.Sub(XZW, 29, 0, 0);
    // 5E8: 004B6ACD 8000033C  msuby.z vf11, vf13, vf11y                    nop
    vu.MsubBc(Z, 11, 13, 11, Fy);
    // 5F0: 01EE49BC 8000033C  mulax.xyzw ACC, vf09, vf14x                  nop
    vu.MulaBc(XYZW, 9, 14, Fx);
    vu.q = q5C0;
    // 5F8: 00400360 804E5B3D  addq.z vf13, vf00, q                         mr32.z vf14, vf11
    vu.AddQ(Z, 13, 0);
    vu.Mr32(Z, 14, 11);
    // 600: 004A0281 802B033D  addy.z vf10, vf00, vf10y                     mr32.w vf11, vf00
    vu.AddBc(Z, 10, 0, 10, Fy);
    vu.Mr32(W, 11, 0);
    // 608: 01EEE8BD 8000033C  madday.xyzw ACC, vf29, vf14y                 nop
    vu.MaddaBc(XYZW, 29, 14, Fy);
    // 610: 008A02C0 808AFB3D  addx.y vf11, vf00, vf10x                     mr32.y vf10, vf31
    vu.AddBc(Y, 11, 0, 10, Fx);
    vu.Mr32(Y, 10, 31);
    // 618: 01EE68BE 8000033C  maddaz.xyzw ACC, vf13, vf14z                 nop
    vu.MaddaBc(XYZW, 13, 14, Fz);
    // 620: 01EB608B 802B033D  maddw.xyzw vf02, vf12, vf11w                 mr32.w vf11, vf00
    vu.MaddBc(XYZW, 2, 12, 11, Fw);
    vu.Mr32(W, 11, 0);
    // 628: 01EB49BC 810BFB3D  mulax.xyzw ACC, vf09, vf11x                  mr32.x vf11, vf31
    vu.MulaBc(XYZW, 9, 11, Fx);
    vu.Mr32(X, 11, 31);
    // 630: 01EAE8BD 8000033C  madday.xyzw ACC, vf29, vf10y                 nop
    vu.MaddaBc(XYZW, 29, 10, Fy);
    // 638: 01EA68BE 8000033C  maddaz.xyzw ACC, vf13, vf10z                 nop
    vu.MaddaBc(XYZW, 13, 10, Fz);
    // 640: 01EB604B 802B033D  maddw.xyzw vf01, vf12, vf11w                 mr32.w vf11, vf00
    vu.MaddBc(XYZW, 1, 12, 11, Fw);
    vu.Mr32(W, 11, 0);
    // 648: 01EB49BC 8000033C  mulax.xyzw ACC, vf09, vf11x                  nop
    vu.MulaBc(XYZW, 9, 11, Fx);
    // 650: 01EBE8BD 8000033C  madday.xyzw ACC, vf29, vf11y                 nop
    vu.MaddaBc(XYZW, 29, 11, Fy);
    // 658: 41EB68BE 8000033C  maddaz[e].xyzw ACC, vf13, vf11z              nop
    vu.MaddaBc(XYZW, 13, 11, Fz);
    // 660: 01EB60CB 8000033C  maddw.xyzw vf03, vf12, vf11w                 nop
    vu.MaddBc(XYZW, 3, 12, 11, Fw);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd548Ranges[] = {{0xA9, 0xCD}};

// The standard set's program at 0x668 (Platform::Math::JointMatrix (a rotation and a scale))
void MicroprogramStd668(Vu0& vu)
{
    u32 q678;
    u32 q6B0;
    u32 q6E8;

    // 668: 81DFFABE 3FB504F3  mula[i].xyz ACC, vf31, vf31                  loi 0x3FB504F3 (1.4142135381698608)
    vu.Mula(XYZ, 31, 31);
    vu.i = 0x3FB504F3;
    // 670: 01E0FB1E 8000033C  muli.xyzw vf12, vf31, i                      nop
    vu.MulI(XYZW, 12, 31);
    // 678: 01DFFA69 807D03BC  madd.xyz vf09, vf31, vf31                    div Q, vf00.w, vf29.x
    vu.Madd(XYZ, 9, 31, 31);
    q678 = vu.DivValue(0, Fw, 29, Fx);
    // 680: 01C00283 8000033C  addw.xyz vf10, vf00, vf00w                   nop
    vu.AddBc(XYZ, 10, 0, 0, Fw);
    // 688: 00E007EC 8000033C  sub.yzw vf31, vf00, vf00                     nop
    vu.Sub(YZW, 31, 0, 0);
    // 690: 01CC62FE 8000033C  opmula.xyz ACC, vf12, vf12                   nop
    vu.Opmula(12, 12);
    // 698: 01CC62CF 802A033D  msubw.xyz vf11, vf12, vf12w                  mr32.w vf10, vf00
    vu.MsubBc(XYZ, 11, 12, 12, Fw);
    vu.Mr32(W, 10, 0);
    // 6A0: 01CC634B 802B033D  maddw.xyz vf13, vf12, vf12w                  mr32.w vf11, vf00
    vu.MaddBc(XYZ, 13, 12, 12, Fw);
    vu.Mr32(W, 11, 0);
    // 6A8: 01C0003F 802C033D  addaw.xyz ACC, vf00, vf00w                   mr32.w vf12, vf00
    vu.AddaBc(XYZ, 0, 0, Fw);
    vu.Mr32(W, 12, 0);
    vu.q = q678;
    // 6B0: 00C950FC 80FD03BC  msubax.yz ACC, vf10, vf09x                   div Q, vf00.w, vf29.y
    vu.MsubaBc(YZ, 10, 9, Fx);
    q6B0 = vu.DivValue(0, Fw, 29, Fy);
    // 6B8: 010950FD 809D5B3D  msubay.x ACC, vf10, vf09y                    mr32.y vf29, vf11
    vu.MsubaBc(X, 10, 9, Fy);
    vu.Mr32(Y, 29, 11);
    // 6C0: 008B0300 803E6B3D  addx.y vf12, vf00, vf11x                     mr32.w vf30, vf13
    vu.AddBc(Y, 12, 0, 11, Fx);
    vu.Mr32(W, 30, 13);
    // 6C8: 004B02C1 810C6B3D  addy.z vf11, vf00, vf11y                     mr32.x vf12, vf13
    vu.AddBc(Z, 11, 0, 11, Fy);
    vu.Mr32(X, 12, 13);
    // 6D0: 0049530D 808B6B3D  msuby.z vf12, vf10, vf09y                    mr32.y vf11, vf13
    vu.MsubBc(Z, 12, 10, 9, Fy);
    vu.Mr32(Y, 11, 13);
    // 6D8: 010952CE 810AEB3D  msubz.x vf11, vf10, vf09z                    mr32.x vf10, vf29
    vu.MsubBc(X, 11, 10, 9, Fz);
    vu.Mr32(X, 10, 29);
    // 6E0: 010007E0 804AF33D  addq.x vf31, vf00, q                         mr32.z vf10, vf30
    vu.AddQ(X, 31, 0);
    vu.Mr32(Z, 10, 30);
    vu.q = q6B0;
    // 6E8: 0089528E 817D03BC  msubz.y vf10, vf10, vf09z                    div Q, vf00.w, vf29.z
    vu.MsubBc(Y, 10, 10, 9, Fz);
    q6E8 = vu.DivValue(0, Fw, 29, Fz);
    // 6F0: 01FE675A 8000033C  mulz.xyzw vf29, vf12, vf30z                  nop
    vu.MulBc(XYZW, 29, 12, 30, Fz);
    // 6F8: 01FE5A58 8000033C  mulx.xyzw vf09, vf11, vf30x                  nop
    vu.MulBc(XYZW, 9, 11, 30, Fx);
    // 700: 016007AC 8000033C  sub.xzw vf30, vf00, vf00                     nop
    vu.Sub(XZW, 30, 0, 0);
    // 708: 01FE5319 8000033C  muly.xyzw vf12, vf10, vf30y                  nop
    vu.MulBc(XYZW, 12, 10, 30, Fy);
    // 710: 008007A0 8000033C  addq.y vf30, vf00, q                         nop
    vu.AddQ(Y, 30, 0);
    // 718: 01A002AC 8000033C  sub.xyw vf10, vf00, vf00                     nop
    vu.Sub(XYW, 10, 0, 0);
    vu.q = q6E8;
    // 720: 004002A0 8000033C  addq.z vf10, vf00, q                         nop
    vu.AddQ(Z, 10, 0);
    // 728: 01E002EC 8000033C  sub.xyzw vf11, vf00, vf00                    nop
    vu.Sub(XYZW, 11, 0, 0);
    // 730: 01ECF9BC 8000033C  mulax.xyzw ACC, vf31, vf12x                  nop
    vu.MulaBc(XYZW, 31, 12, Fx);
    // 738: 01ECF0BD 8000033C  madday.xyzw ACC, vf30, vf12y                 nop
    vu.MaddaBc(XYZW, 30, 12, Fy);
    // 740: 01EC50BE 8000033C  maddaz.xyzw ACC, vf10, vf12z                 nop
    vu.MaddaBc(XYZW, 10, 12, Fz);
    // 748: 01EC588B 8000033C  maddw.xyzw vf02, vf11, vf12w                 nop
    vu.MaddBc(XYZW, 2, 11, 12, Fw);
    // 750: 01E9F9BC 8000033C  mulax.xyzw ACC, vf31, vf09x                  nop
    vu.MulaBc(XYZW, 31, 9, Fx);
    // 758: 01E9F0BD 8000033C  madday.xyzw ACC, vf30, vf09y                 nop
    vu.MaddaBc(XYZW, 30, 9, Fy);
    // 760: 01E950BE 8000033C  maddaz.xyzw ACC, vf10, vf09z                 nop
    vu.MaddaBc(XYZW, 10, 9, Fz);
    // 768: 01E9584B 8000033C  maddw.xyzw vf01, vf11, vf09w                 nop
    vu.MaddBc(XYZW, 1, 11, 9, Fw);
    // 770: 01FDF9BC 8000033C  mulax.xyzw ACC, vf31, vf29x                  nop
    vu.MulaBc(XYZW, 31, 29, Fx);
    // 778: 01FDF0BD 8000033C  madday.xyzw ACC, vf30, vf29y                 nop
    vu.MaddaBc(XYZW, 30, 29, Fy);
    // 780: 41FD50BE 8000033C  maddaz[e].xyzw ACC, vf10, vf29z              nop
    vu.MaddaBc(XYZW, 10, 29, Fz);
    // 788: 01FD58CB 8000033C  maddw.xyzw vf03, vf11, vf29w                 nop
    vu.MaddBc(XYZW, 3, 11, 29, Fw);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd668Ranges[] = {{0xCD, 0xF2}};

// The standard set's program at 0x790 (Platform::Math::JointMatrix, MultiplyByParent)
void MicroprogramStd790(Vu0& vu)
{
    // 790: 01E329BC 8000033C  mulax.xyzw ACC, vf05, vf03x                  nop
    vu.MulaBc(XYZW, 5, 3, Fx);
    // 798: 01E330BD 8000033C  madday.xyzw ACC, vf06, vf03y                 nop
    vu.MaddaBc(XYZW, 6, 3, Fy);
    // 7A0: 01E338BE 8000033C  maddaz.xyzw ACC, vf07, vf03z                 nop
    vu.MaddaBc(XYZW, 7, 3, Fz);
    // 7A8: 01E340CB 8000033C  maddw.xyzw vf03, vf08, vf03w                 nop
    vu.MaddBc(XYZW, 3, 8, 3, Fw);
    // 7B0: 01E229BC 8000033C  mulax.xyzw ACC, vf05, vf02x                  nop
    vu.MulaBc(XYZW, 5, 2, Fx);
    // 7B8: 01E230BD 8000033C  madday.xyzw ACC, vf06, vf02y                 nop
    vu.MaddaBc(XYZW, 6, 2, Fy);
    // 7C0: 01E238BE 8000033C  maddaz.xyzw ACC, vf07, vf02z                 nop
    vu.MaddaBc(XYZW, 7, 2, Fz);
    // 7C8: 01E2408B 8000033C  maddw.xyzw vf02, vf08, vf02w                 nop
    vu.MaddBc(XYZW, 2, 8, 2, Fw);
    // 7D0: 01E129BC 8000033C  mulax.xyzw ACC, vf05, vf01x                  nop
    vu.MulaBc(XYZW, 5, 1, Fx);
    // 7D8: 01E130BD 8000033C  madday.xyzw ACC, vf06, vf01y                 nop
    vu.MaddaBc(XYZW, 6, 1, Fy);
    // 7E0: 01E138BE 8000033C  maddaz.xyzw ACC, vf07, vf01z                 nop
    vu.MaddaBc(XYZW, 7, 1, Fz);
    // 7E8: 01E1404B 8000033C  maddw.xyzw vf01, vf08, vf01w                 nop
    vu.MaddBc(XYZW, 1, 8, 1, Fw);
    // 7F0: 01C429BC 8000033C  mulax.xyz ACC, vf05, vf04x                   nop
    vu.MulaBc(XYZ, 5, 4, Fx);
    // 7F8: 01C430BD 8000033C  madday.xyz ACC, vf06, vf04y                  nop
    vu.MaddaBc(XYZ, 6, 4, Fy);
    // 800: 41C438BE 8024033C  maddaz[e].xyz ACC, vf07, vf04z               move.w vf04, vf00
    vu.MaddaBc(XYZ, 7, 4, Fz);
    vu.Move(W, 4, 0);
    // 808: 01C0410B 8000033C  maddw.xyz vf04, vf08, vf00w                  nop
    vu.MaddBc(XYZ, 4, 8, 0, Fw);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd790Ranges[] = {{0xF2, 0x102}};

// The standard set's program at 0x818 (Platform::Math::EulerRotation)
void MicroprogramStd818(Vu0& vu)
{
    // 818: 81C0F83C 3F000000  addax[i].xyz ACC, vf31, vf00x                loi 0x3F000000 (0.5)
    vu.AddaBc(XYZ, 31, 0, Fx);
    vu.i = 0x3F000000;
    // 820: 01DEF8FF 8000033C  msubaw.xyz ACC, vf31, vf30w                  nop
    vu.MsubaBc(XYZ, 31, 30, Fw);
    // 828: 01DEF08B 8000033C  maddw.xyz vf02, vf30, vf30w                  nop
    vu.MaddBc(XYZ, 2, 30, 30, Fw);
    // 830: 01C0E83C 8101133D  addax.xyz ACC, vf29, vf00x                   mr32.x vf01, vf02
    vu.AddaBc(XYZ, 29, 0, Fx);
    vu.Mr32(X, 1, 2);
    // 838: 0140179E 8000033C  muli.xz vf30, vf02, i                        nop
    vu.MulI(XZ, 30, 2);
    // 840: 01DEE8FF 8000033C  msubaw.xyz ACC, vf29, vf30w                  nop
    vu.MsubaBc(XYZ, 29, 30, Fw);
    // 848: 01DEE08B 8000033C  maddw.xyz vf02, vf28, vf30w                  nop
    vu.MaddBc(XYZ, 2, 28, 30, Fw);
    // 850: 01000F1E 8000033C  muli.x vf28, vf01, i                         nop
    vu.MulI(X, 28, 1);
    // 858: 009E0780 8000033C  addx.y vf30, vf00, vf30x                     nop
    vu.AddBc(Y, 30, 0, 30, Fx);
    // 860: 803E079A 3FC90FD8  mulz[i].w vf30, vf00, vf30z                  loi 0x3FC90FD8 (1.570796012878418)
    vu.MulBc(W, 30, 0, 30, Fz);
    vu.i = 0x3FC90FD8;
    // 868: 8140F7A6 3FC90FD8  subi[i].xz vf30, vf30, i                     loi 0x3FC90FD8 (1.570796012878418)
    vu.SubI(XZ, 30, 30);
    vu.i = 0x3FC90FD8;
    // 870: 01FDF1FD 8000033C  abs.xyzw vf29, vf30                          nop
    vu.Abs(XYZW, 29, 30);
    // 878: 01E00053 8000033C  maxw.xyzw vf01, vf00, vf00w                  nop
    vu.MaxBc(XYZW, 1, 0, 0, Fw);
    // 880: 000002FF 8000033C  nop                                          nop
    // 888: 8100E7A6 BE22F987  subi[i].x vf30, vf28, i                      loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(X, 30, 28);
    vu.i = 0xBE22F987;
    // 890: 81E0E9FE 4B400000  mulai[i].xyzw ACC, vf29, i                   loi 0x4B400000 (12582912.0)
    vu.MulaI(XYZW, 29);
    vu.i = 0x4B400000;
    // 898: 01E00A7F 8000033C  msubai.xyzw ACC, vf01, i                     nop
    vu.MsubaI(XYZW, 1);
    // 8A0: 81E00A3F BE22F987  maddai[i].xyzw ACC, vf01, i                  loi 0xBE22F987 (-0.1591549962759018)
    vu.MaddaI(XYZW, 1);
    vu.i = 0xBE22F987;
    // 8A8: 81E0EA7F 3F000000  msubai[i].xyzw ACC, vf29, i                  loi 0x3F000000 (0.5)
    vu.MsubaI(XYZW, 29);
    vu.i = 0x3F000000;
    // 8B0: 81E00867 3E800000  msubi[i].xyzw vf01, vf01, i                  loi 0x3E800000 (0.25)
    vu.MsubI(XYZW, 1, 1);
    vu.i = 0x3E800000;
    // 8B8: 009C0780 8000033C  addx.y vf30, vf00, vf28x                     nop
    vu.AddBc(Y, 30, 0, 28, Fx);
    // 8C0: 01FC09FD 8000033C  abs.xyzw vf28, vf01                          nop
    vu.Abs(XYZW, 28, 1);
    // 8C8: 0181F1FD 8000033C  abs.xy vf01, vf30                            nop
    vu.Abs(XY, 1, 30);
    // 8D0: 01800793 8000033C  maxw.xy vf30, vf00, vf00w                    nop
    vu.MaxBc(XY, 30, 0, 0, Fw);
    // 8D8: 81E0E726 BE22F987  subi[i].xyzw vf28, vf28, i                   loi 0xBE22F987 (-0.1591549962759018)
    vu.SubI(XYZW, 28, 28);
    vu.i = 0xBE22F987;
    // 8E0: 818009FE 4B400000  mulai[i].xy ACC, vf01, i                     loi 0x4B400000 (12582912.0)
    vu.MulaI(XY, 1);
    vu.i = 0x4B400000;
    // 8E8: 0180F27F 8000033C  msubai.xy ACC, vf30, i                       nop
    vu.MsubaI(XY, 30);
    // 8F0: 8180F23F BE22F987  maddai[i].xy ACC, vf30, i                    loi 0xBE22F987 (-0.1591549962759018)
    vu.MaddaI(XY, 30);
    vu.i = 0xBE22F987;
    // 8F8: 81800A7F 3F000000  msubai[i].xy ACC, vf01, i                    loi 0x3F000000 (0.5)
    vu.MsubaI(XY, 1);
    vu.i = 0x3F000000;
    // 900: 0180F067 8000033C  msubi.xy vf01, vf30, i                       nop
    vu.MsubI(XY, 1, 30);
    // 908: 000002FF 8000033C  nop                                          nop
    // 910: 000002FF 8000033C  nop                                          nop
    // 918: 01FCE7AA 8000033C  mul.xyzw vf30, vf28, vf28                    nop
    vu.Mul(XYZW, 30, 28, 28);
    // 920: 019D09FD 8000033C  abs.xy vf29, vf01                            nop
    vu.Abs(XY, 29, 1);
    // 928: 81FEF06A 3E800000  mul[i].xyzw vf01, vf30, vf30                 loi 0x3E800000 (0.25)
    vu.Mul(XYZW, 1, 30, 30);
    vu.i = 0x3E800000;
    // 930: 8180EF66 C2255DE0  subi[i].xy vf29, vf29, i                     loi 0xC2255DE0 (-41.3416748046875)
    vu.SubI(XY, 29, 29);
    vu.i = 0xC2255DE0;
    // 938: 81E0E19E C2992661  muli[i].xyzw vf06, vf28, i                   loi 0xC2992661 (-76.57495880126953)
    vu.MulI(XYZW, 6, 28);
    vu.i = 0xC2992661;
    // 940: 81E0E31E 42A33457  muli[i].xyzw vf12, vf28, i                   loi 0x42A33457 (81.60222625732422)
    vu.MulI(XYZW, 12, 28);
    vu.i = 0x42A33457;
    // 948: 01E0E15E 8000033C  muli.xyzw vf05, vf28, i                      nop
    vu.MulI(XYZW, 5, 28);
    // 950: 819DEFEA 421ED7B7  mul[i].xy vf31, vf29, vf29                   loi 0x421ED7B7 (39.71065902709961)
    vu.Mul(XY, 31, 29, 29);
    vu.i = 0x421ED7B7;
    // 958: 01E0E11E 8000033C  muli.xyzw vf04, vf28, i                      nop
    vu.MulI(XYZW, 4, 28);
    // 960: 81FE632A C2992661  mul[i].xyzw vf12, vf12, vf30                 loi 0xC2992661 (-76.57495880126953)
    vu.Mul(XYZW, 12, 12, 30);
    vu.i = 0xC2992661;
    // 968: 0180E9DE 8000033C  muli.xy vf07, vf29, i                        nop
    vu.MulI(XY, 7, 29);
    // 970: 819FF8EA C2255DE0  mul[i].xy vf03, vf31, vf31                   loi 0xC2255DE0 (-41.3416748046875)
    vu.Mul(XY, 3, 31, 31);
    vu.i = 0xC2255DE0;
    // 978: 0180EA1E 8000033C  muli.xy vf08, vf29, i                        nop
    vu.MulI(XY, 8, 29);
    // 980: 01E10B6A 8000033C  mul.xyzw vf13, vf01, vf01                    nop
    vu.Mul(XYZW, 13, 1, 1);
    // 988: 819F3AEA 42A33457  mul[i].xy vf11, vf07, vf31                   loi 0x42A33457 (81.60222625732422)
    vu.Mul(XY, 11, 7, 31);
    vu.i = 0x42A33457;
    // 990: 0180EA9E 8000033C  muli.xy vf10, vf29, i                        nop
    vu.MulI(XY, 10, 29);
    // 998: 818319EA 421ED7B7  mul[i].xy vf07, vf03, vf03                   loi 0x421ED7B7 (39.71065902709961)
    vu.Mul(XY, 7, 3, 3);
    vu.i = 0x421ED7B7;
    // 9A0: 0180EA5E 8000033C  muli.xy vf09, vf29, i                        nop
    vu.MulI(XY, 9, 29);
    // 9A8: 01FE32BE 8000033C  mula.xyzw ACC, vf06, vf30                    nop
    vu.Mula(XYZW, 6, 30);
    // 9B0: 01E162BD 8000033C  madda.xyzw ACC, vf12, vf01                   nop
    vu.Madda(XYZW, 12, 1);
    // 9B8: 81E12ABD 40C90FDA  madda[i].xyzw ACC, vf05, vf01                loi 0x40C90FDA (6.283185005187988)
    vu.Madda(XYZW, 5, 1);
    vu.i = 0x40C90FDA;
    // 9C0: 01E0E23F 8000033C  maddai.xyzw ACC, vf28, i                     nop
    vu.MaddaI(XYZW, 28);
    // 9C8: 01ED27A9 8000033C  madd.xyzw vf30, vf04, vf13                   nop
    vu.Madd(XYZW, 30, 4, 13);
    // 9D0: 019F42BE 8000033C  mula.xy ACC, vf08, vf31                      nop
    vu.Mula(XY, 8, 31);
    // 9D8: 01835ABD 8000033C  madda.xy ACC, vf11, vf03                     nop
    vu.Madda(XY, 11, 3);
    // 9E0: 818352BD 40C90FDA  madda[i].xy ACC, vf10, vf03                  loi 0x40C90FDA (6.283185005187988)
    vu.Madda(XY, 10, 3);
    vu.i = 0x40C90FDA;
    // 9E8: 0180EA3F 8000033C  maddai.xy ACC, vf29, i                       nop
    vu.MaddaI(XY, 29);
    // 9F0: 01874F29 8000033C  madd.xy vf28, vf09, vf07                     nop
    vu.Madd(XY, 28, 9, 7);
    // 9F8: 019EF11A 8000033C  mulz.xy vf04, vf30, vf30z                    nop
    vu.MulBc(XY, 4, 30, 30, Fz);
    // A00: 019EF75B 8000033C  mulw.xy vf29, vf30, vf30w                    nop
    vu.MulBc(XY, 29, 30, 30, Fw);
    // A08: 019C2059 8000033C  muly.xy vf01, vf04, vf28y                    nop
    vu.MulBc(XY, 1, 4, 28, Fy);
    // A10: 019CE958 8000033C  mulx.xy vf05, vf29, vf28x                    nop
    vu.MulBc(XY, 5, 29, 28, Fx);
    // A18: 019C2798 8000033C  mulx.xy vf30, vf04, vf28x                    nop
    vu.MulBc(XY, 30, 4, 28, Fx);
    // A20: 019CEF59 8000033C  muly.xy vf29, vf29, vf28y                    nop
    vu.MulBc(XY, 29, 29, 28, Fy);
    // A28: 00E0072C 8000033C  sub.yzw vf28, vf00, vf00                     nop
    vu.Sub(YZW, 28, 0, 0);
    // A30: 01050F01 8000033C  addy.x vf28, vf01, vf05y                     nop
    vu.AddBc(X, 28, 1, 5, Fy);
    // A38: 00850904 8000033C  subx.y vf04, vf01, vf05x                     nop
    vu.SubBc(Y, 4, 1, 5, Fx);
    // A40: 009EEF40 8000033C  addx.y vf29, vf29, vf30x                     nop
    vu.AddBc(Y, 29, 29, 30, Fx);
    // A48: 011EEF85 8000033C  suby.x vf30, vf29, vf30y                     nop
    vu.SubBc(X, 30, 29, 30, Fy);
    // A50: 009CE040 8000033C  addx.y vf01, vf28, vf28x                     nop
    vu.AddBc(Y, 1, 28, 28, Fx);
    // A58: 0044E041 8000033C  addy.z vf01, vf28, vf04y                     nop
    vu.AddBc(Z, 1, 28, 4, Fy);
    // A60: 403DE041 8000033C  addy[e].w vf01, vf28, vf29y                  nop
    vu.AddBc(W, 1, 28, 29, Fy);
    // A68: 011EF06B 8022033C  max.x vf01, vf30, vf30                       move.w vf02, vf00
    vu.Max(X, 1, 30, 30);
    vu.Move(W, 2, 0);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStd818Ranges[] = {{0x103, 0x14E}};

// The standard set's program at 0xA70 (Platform::Math::TurnRotation)
void MicroprogramStdA70(Vu0& vu)
{
    // A70: 01E1F8AA 8000033C  mul.xyzw vf02, vf31, vf01                    nop
    vu.Mul(XYZW, 2, 31, 1);
    // A78: 01C1FAFE 8000033C  opmula.xyz ACC, vf31, vf01                   nop
    vu.Opmula(31, 1);
    // A80: 01C1F8BF 8000033C  maddaw.xyz ACC, vf31, vf01w                  nop
    vu.MaddaBc(XYZ, 31, 1, Fw);
    // A88: 01DF08BF 8000033C  maddaw.xyz ACC, vf01, vf31w                  nop
    vu.MaddaBc(XYZ, 1, 31, Fw);
    // A90: 0022107E 8000033C  subaz.w ACC, vf02, vf02z                     nop
    vu.SubaBc(W, 2, 2, Fz);
    // A98: 002200FD 8000033C  msubay.w ACC, vf00, vf02y                    nop
    vu.MsubaBc(W, 0, 2, Fy);
    // AA0: 41DF086E 8000033C  opmsub[e].xyz vf01, vf01, vf31               nop
    vu.Opmsub(1, 1, 31);
    // AA8: 0022004C 8000033C  msubx.w vf01, vf00, vf02x                    nop
    vu.MsubBc(W, 1, 0, 2, Fx);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStdA70Ranges[] = {{0x14E, 0x156}};

// The standard set's program at 0xAB8 (Platform::Math::StartRayTriangle)
void MicroprogramStdAB8(Vu0& vu)
{
    u32 qB18;
    u32 qB68;
    u32 qBC0;
    u32 macBF8;
    u32 macC00;
    u32 qC28;
    u32 qC78;
    u32 macCD8;
    u32 macD50;

    // AB8: 01C111AC 10010080  sub.xyz vf06, vf02, vf01                     iaddiu vi01, vi00, 0x80
    vu.Sub(XYZ, 6, 2, 1);
    vu.Iaddiu(1, 0, 0x80);
    // AC0: 01C119EC 8000033C  sub.xyz vf07, vf03, vf01                     nop
    vu.Sub(XYZ, 7, 3, 1);
    // AC8: 01C732FE 8000033C  opmula.xyz ACC, vf06, vf07                   nop
    vu.Opmula(6, 7);
    // AD0: 01C63A2E 8000033C  opmsub.xyz vf08, vf07, vf06                  nop
    vu.Opmsub(8, 7, 6);
    // AD8: 01C841EA 8000033C  mul.xyz vf07, vf08, vf08                     nop
    vu.Mul(XYZ, 7, 8, 8);
    // AE0: 000002FF 8000033C  nop                                          nop
    // AE8: 000002FF 8000033C  nop                                          nop
    // AF0: 01C218AC 8000033C  sub.xyz vf02, vf03, vf02                     nop
    vu.Sub(XYZ, 2, 3, 2);
    // AF8: 010739C1 8000033C  addy.x vf07, vf07, vf07y                     nop
    vu.AddBc(X, 7, 7, 7, Fy);
    // B00: 01C242FE 8000033C  opmula.xyz ACC, vf08, vf02                   nop
    vu.Opmula(8, 2);
    // B08: 01073AC2 8000033C  addz.x vf11, vf07, vf07z                     nop
    vu.AddBc(X, 11, 7, 7, Fz);
    // B10: 01C810AE 8000033C  opmsub.xyz vf02, vf02, vf08                  nop
    vu.Opmsub(2, 2, 8);
    // B18: 01C30FEC 806B03BE  sub.xyz vf31, vf01, vf03                     rsqrt Q, vf00.w, vf11.x
    vu.Sub(XYZ, 31, 1, 3);
    qB18 = vu.RsqrtValue(0, Fw, 11, Fx);
    // B20: 01C212AA 8000033C  mul.xyz vf10, vf02, vf02                     nop
    vu.Mul(XYZ, 10, 2, 2);
    // B28: 010A5281 8000033C  addy.x vf10, vf10, vf10y                     nop
    vu.AddBc(X, 10, 10, 10, Fy);
    // B30: 000002FF 8000033C  nop                                          nop
    // B38: 000002FF 8000033C  nop                                          nop
    // B40: 01C642FE 8000033C  opmula.xyz ACC, vf08, vf06                   nop
    vu.Opmula(8, 6);
    // B48: 010A5282 8000033C  addz.x vf10, vf10, vf10z                     nop
    vu.AddBc(X, 10, 10, 10, Fz);
    // B50: 01C831AE 8000033C  opmsub.xyz vf06, vf06, vf08                  nop
    vu.Opmsub(6, 6, 8);
    // B58: 01DF42FE 8000033C  opmula.xyz ACC, vf08, vf31                   nop
    vu.Opmula(8, 31);
    // B60: 01C8FFEE 8000033C  opmsub.xyz vf31, vf31, vf08                  nop
    vu.Opmsub(31, 31, 8);
    vu.q = qB18;
    // B68: 01C0421C 806A03BE  mulq.xyz vf08, vf08, q                       rsqrt Q, vf00.w, vf10.x
    vu.MulQ(XYZ, 8, 8);
    qB68 = vu.RsqrtValue(0, Fw, 10, Fx);
    // B70: 01C6326A 8000033C  mul.xyz vf09, vf06, vf06                     nop
    vu.Mul(XYZ, 9, 6, 6);
    // B78: 01DFF9EA 8000033C  mul.xyz vf07, vf31, vf31                     nop
    vu.Mul(XYZ, 7, 31, 31);
    // B80: 01C142EA 8000033C  mul.xyz vf11, vf08, vf01                     nop
    vu.Mul(XYZ, 11, 8, 1);
    // B88: 01094A41 8000033C  addy.x vf09, vf09, vf09y                     nop
    vu.AddBc(X, 9, 9, 9, Fy);
    // B90: 01C822AA 8000033C  mul.xyz vf10, vf04, vf08                     nop
    vu.Mul(XYZ, 10, 4, 8);
    // B98: 01C82A2A 8000033C  mul.xyz vf08, vf05, vf08                     nop
    vu.Mul(XYZ, 8, 5, 8);
    // BA0: 010B5B01 8000033C  addy.x vf12, vf11, vf11y                     nop
    vu.AddBc(X, 12, 11, 11, Fy);
    // BA8: 01094AC2 8000033C  addz.x vf11, vf09, vf09z                     nop
    vu.AddBc(X, 11, 9, 9, Fz);
    // BB0: 010A5281 8000033C  addy.x vf10, vf10, vf10y                     nop
    vu.AddBc(X, 10, 10, 10, Fy);
    // BB8: 010B6242 803F033D  addz.x vf09, vf12, vf11z                     mr32.w vf31, vf00
    vu.AddBc(X, 9, 12, 11, Fz);
    vu.Mr32(W, 31, 0);
    vu.q = qB68;
    // BC0: 01084201 806B03BE  addy.x vf08, vf08, vf08y                     rsqrt Q, vf00.w, vf11.x
    vu.AddBc(X, 8, 8, 8, Fy);
    qBC0 = vu.RsqrtValue(0, Fw, 11, Fx);
    // BC8: 01C0109C 8000033C  mulq.xyz vf02, vf02, q                       nop
    vu.MulQ(XYZ, 2, 2);
    // BD0: 010A5282 8000033C  addz.x vf10, vf10, vf10z                     nop
    vu.AddBc(X, 10, 10, 10, Fz);
    // BD8: 0029FFC4 8000033C  subx.w vf31, vf31, vf09x                     nop
    vu.SubBc(W, 31, 31, 9, Fx);
    // BE0: 010842C2 8000033C  addz.x vf11, vf08, vf08z                     nop
    vu.AddBc(X, 11, 8, 8, Fz);
    // BE8: 01073A41 8000033C  addy.x vf09, vf07, vf07y                     nop
    vu.AddBc(X, 9, 7, 7, Fy);
    // BF0: 01C3122A 8000033C  mul.xyz vf08, vf02, vf03                     nop
    vu.Mul(XYZ, 8, 2, 3);
    // BF8: 011F50C3 8000033C  addw.x vf03, vf10, vf31w                     nop
    vu.AddBc(X, 3, 10, 31, Fw);
    macBF8 = vu.mac;
    // C00: 011F59C3 8000043E  addw.x vf07, vf11, vf31w                     rinit R, vf00.x
    vu.AddBc(X, 7, 11, 31, Fw);
    vu.Rinit(0, Fx);
    macC00 = vu.mac;
    // C08: 01074A42 810A043D  addz.x vf09, vf09, vf07z                     rget.x vf10, R
    vu.AddBc(X, 9, 9, 7, Fz);
    vu.Rget(X, 10);
    // C10: 0108403D 803F033D  adday.x ACC, vf08, vf08y                     mr32.w vf31, vf00
    vu.AddaBc(X, 8, 8, Fy);
    vu.Mr32(W, 31, 0);
    // C18: 010319FD 34020800  abs.x vf03, vf03                             fmand vi02, vi01
    vu.Abs(X, 3, 3);
    vu.Fmand(2, 1, macBF8);
    // C20: 0020016B 34010800  max.w vf05, vf00, vf00                       fmand vi01, vi01
    vu.Max(W, 5, 0, 0);
    vu.Fmand(1, 1, macC00);
    vu.q = qBC0;
    // C28: 0108520A 806903BE  maddz.x vf08, vf10, vf08z                    rsqrt Q, vf00.w, vf09.x
    vu.MaddBc(X, 8, 10, 8, Fz);
    qC28 = vu.RsqrtValue(0, Fw, 9, Fx);
    // C30: 000002FF 8000033C  nop                                          nop
    // C38: 000002FF 8000033C  nop                                          nop
    // C40: 000002FF 8000033C  nop                                          nop
    // C48: 010A39FD 8000033C  abs.x vf10, vf07                             nop
    vu.Abs(X, 10, 7);
    // C50: 010A1AA8 8000033C  add.x vf10, vf03, vf10                       nop
    vu.Add(X, 10, 3, 10);
    // C58: 000002FF 8000033C  nop                                          nop
    // C60: 000002FF 8000033C  nop                                          nop
    // C68: 000002FF 8000033C  nop                                          nop
    // C70: 01C0319C 8000033C  mulq.xyz vf06, vf06, q                       nop
    vu.MulQ(XYZ, 6, 6);
    vu.q = qC28;
    // C78: 01C0F8DC 800A1BBC  mulq.xyz vf03, vf31, q                       div Q, vf03.x, vf10.x
    vu.vf[Vu0::Temporary] = vu.vf[3]; // the lower instruction reads it as it was
    vu.MulQ(XYZ, 3, 31);
    qC78 = vu.DivValue(Vu0::Temporary, Fx, 10, Fx);
    // C80: 000002FF 8000033C  nop                                          nop
    // C88: 0028FFC4 8024033D  subx.w vf31, vf31, vf08x                     mr32.w vf04, vf00
    vu.SubBc(W, 31, 31, 8, Fx);
    vu.Mr32(W, 4, 0);
    // C90: 01C11AAA 8000043E  mul.xyz vf10, vf03, vf01                     rinit R, vf00.x
    vu.Mul(XYZ, 10, 3, 1);
    vu.Rinit(0, Fx);
    // C98: 01C131EA 8108043D  mul.xyz vf07, vf06, vf01                     rget.x vf08, R
    vu.Mul(XYZ, 7, 6, 1);
    vu.Rget(X, 8);
    // CA0: 01E0203C 8000043E  addax.xyzw ACC, vf04, vf00x                  rinit R, vf00.x
    vu.AddaBc(XYZW, 4, 0, Fx);
    vu.Rinit(0, Fx);
    vu.q = qC78;
    // CA8: 01E0227D 8109043D  msubaq.xyzw ACC, vf04, q                     rget.x vf09, R
    vu.MsubaQ(XYZW, 4);
    vu.Rget(X, 9);
    // CB0: 01E02FE1 8041FB3D  maddq.xyzw vf31, vf05, q                     mr32.z vf01, vf31
    vu.vf[Vu0::Temporary] = vu.vf[31]; // the lower instruction reads it as it was
    vu.MaddQ(XYZW, 31, 5);
    vu.Mr32(Z, 1, Vu0::Temporary);
    // CB8: 0107383D 8000033C  adday.x ACC, vf07, vf07y                     nop
    vu.AddaBc(X, 7, 7, Fy);
    // CC0: 0107420A 8022033D  maddz.x vf08, vf08, vf07z                    mr32.w vf02, vf00
    vu.MaddBc(X, 8, 8, 7, Fz);
    vu.Mr32(W, 2, 0);
    // CC8: 010A503D 8021033D  adday.x ACC, vf10, vf10y                     mr32.w vf01, vf00
    vu.AddaBc(X, 10, 10, Fy);
    vu.Mr32(W, 1, 0);
    // CD0: 010A49CA 800208B0  maddz.x vf07, vf09, vf10z                    iadd vi02, vi01, vi02
    vu.MaddBc(X, 7, 9, 10, Fz);
    vu.Iadd(2, 1, 2);
    // CD8: 0026F82C 8081333D  sub.w vf00, vf31, vf06                       mr32.y vf01, vf06
    vu.Sub(W, 0, 31, 6);
    vu.Mr32(Y, 1, 6);
    macCD8 = vu.mac;
    // CE0: 00281084 8101333D  subx.w vf02, vf02, vf08x                     mr32.x vf01, vf06
    vu.SubBc(W, 2, 2, 8, Fx);
    vu.Mr32(X, 1, 6);
    // CE8: 000002FF 8086133D  nop                                          mr32.y vf06, vf02
    vu.Mr32(Y, 6, 2);
    // CF0: 00270984 10010010  subx.w vf06, vf01, vf07x                     iaddiu vi01, vi00, 0x10
    vu.SubBc(W, 6, 1, 7, Fx);
    vu.Iaddiu(1, 0, 0x10);
    // CF8: 00430081 34010800  addy.z vf02, vf00, vf03y                     fmand vi01, vi01
    vu.AddBc(Z, 2, 0, 3, Fy);
    vu.Fmand(1, 1, macCD8);
    // D00: 010201C3 80461B3C  addw.x vf07, vf00, vf02w                     move.z vf06, vf03
    vu.AddBc(X, 7, 0, 2, Fw);
    vu.Move(Z, 6, 3);
    // D08: 004300C0 80870B3D  addx.z vf03, vf00, vf03x                     mr32.y vf07, vf01
    vu.AddBc(Z, 3, 0, 3, Fx);
    vu.Mr32(Y, 7, 1);
    // D10: 010630EB 8047333D  max.x vf03, vf06, vf06                       mr32.z vf07, vf06
    vu.Max(X, 3, 6, 6);
    vu.Mr32(Z, 7, 6);
    // D18: 008200C0 8000033C  addx.y vf03, vf00, vf02x                     nop
    vu.AddBc(Y, 3, 0, 2, Fx);
    // D20: 010108AB 8000033C  max.x vf02, vf01, vf01                       nop
    vu.Max(X, 2, 1, 1);
    // D28: 01010181 8000043E  addy.x vf06, vf00, vf01y                     rinit R, vf00.x
    vu.AddBc(X, 6, 0, 1, Fy);
    vu.Rinit(0, Fx);
    // D30: 01C039BF 81C1043D  mulaw.xyz ACC, vf07, vf00w                   rget.xyz vf01, R
    vu.MulaBc(XYZ, 7, 0, Fw);
    vu.Rget(XYZ, 1);
    // D38: 81DF18BC 3DCCCCCD  maddax[i].xyz ACC, vf03, vf31x               loi 0x3DCCCCCD (0.10000000149011612)
    vu.MaddaBc(XYZ, 3, 31, Fx);
    vu.i = 0x3DCCCCCD;
    // D40: 01DF10BD 800110B0  madday.xyz ACC, vf02, vf31y                  iadd vi02, vi02, vi01
    vu.MaddaBc(XYZ, 2, 31, Fy);
    vu.Iadd(2, 2, 1);
    // D48: 01DF30BE 800210B0  maddaz.xyz ACC, vf06, vf31z                  iadd vi02, vi02, vi02
    vu.MaddaBc(XYZ, 6, 31, Fz);
    vu.Iadd(2, 2, 2);
    // D50: 01C00B63 800210B0  maddi.xyz vf13, vf01, i                      iadd vi02, vi02, vi02
    vu.MaddI(XYZ, 13, 1);
    vu.Iadd(2, 2, 2);
    macD50 = vu.mac;
    // D58: 000002FF 800210B0  nop                                          iadd vi02, vi02, vi02
    vu.Iadd(2, 2, 2);
    // D60: 000002FF 800210B0  nop                                          iadd vi02, vi02, vi02
    vu.Iadd(2, 2, 2);
    // D68: 000002FF 100100E0  nop                                          iaddiu vi01, vi00, 0xE0
    vu.Iaddiu(1, 0, 0xE0);
    // D70: 41C069FD 34010800  abs[e].xyz vf00, vf13                        fmand vi01, vi01
    vu.Abs(XYZ, 0, 13);
    vu.Fmand(1, 1, macD50);
    // D78: 000002FF 800208B5  nop                                          ior vi02, vi01, vi02
    vu.Ior(2, 1, 2);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramStdAB8Ranges[] = {{0x157, 0x1B0}};

// The culling set's program at 0xA08 (Platform::Math::ParticleBlockView)
void MicroprogramCullA08(Vu0& vu)
{
    u32 macA78;
    u32 qA80;
    u32 macA98;
    u32 qAD8;
    u32 macAE8;
    u32 macAF0;

    // A08: 000002FF 01FB080E  nop                                          lq.xyzw vf27, 14(vi01)
    vu.Lq(XYZW, 27, 1, 14);
    // A10: 01FB36EC 01E10800  sub.xyzw vf27, vf06, vf27                    lq.xyzw vf01, 0(vi01)
    vu.Sub(XYZW, 27, 6, 27);
    vu.Lq(XYZW, 1, 1, 0);
    // A18: 000002FF 8000033C  nop                                          nop
    // A20: 000002FF 01FA0801  nop                                          lq.xyzw vf26, 1(vi01)
    vu.Lq(XYZW, 26, 1, 1);
    // A28: 01FBDFAA 01FC0804  mul.xyzw vf30, vf27, vf27                    lq.xyzw vf28, 4(vi01)
    vu.Mul(XYZW, 30, 27, 27);
    vu.Lq(XYZW, 28, 1, 4);
    // A30: 01E209BC 01E10802  mulax.xyzw ACC, vf01, vf02x                  lq.xyzw vf01, 2(vi01)
    vu.MulaBc(XYZW, 1, 2, Fx);
    vu.Lq(XYZW, 1, 1, 2);
    // A38: 01E2D0BD 01FA0803  madday.xyzw ACC, vf26, vf02y                 lq.xyzw vf26, 3(vi01)
    vu.MaddaBc(XYZW, 26, 2, Fy);
    vu.Lq(XYZW, 26, 1, 3);
    // A40: 011EF781 01EA0809  addy.x vf30, vf30, vf30y                     lq.xyzw vf10, 9(vi01)
    vu.AddBc(X, 30, 30, 30, Fy);
    vu.Lq(XYZW, 10, 1, 9);
    // A48: 01E20F4A 01E80805  maddz.xyzw vf29, vf01, vf02z                 lq.xyzw vf08, 5(vi01)
    vu.MaddBc(XYZW, 29, 1, 2, Fz);
    vu.Lq(XYZW, 8, 1, 5);
    // A50: 01E6D1BC 01E90806  mulax.xyzw ACC, vf26, vf06x                  lq.xyzw vf09, 6(vi01)
    vu.MulaBc(XYZW, 26, 6, Fx);
    vu.Lq(XYZW, 9, 1, 6);
    // A58: 01E6E0BD 01E10808  madday.xyzw ACC, vf28, vf06y                 lq.xyzw vf01, 8(vi01)
    vu.MaddaBc(XYZW, 28, 6, Fy);
    vu.Lq(XYZW, 1, 1, 8);
    // A60: 011EF782 8000033C  addz.x vf30, vf30, vf30z                     nop
    vu.AddBc(X, 30, 30, 30, Fz);
    // A68: 01E640BE 8000033C  maddaz.xyzw ACC, vf08, vf06z                 nop
    vu.MaddaBc(XYZW, 8, 6, Fz);
    // A70: 01E048BF 100200F0  maddaw.xyzw ACC, vf09, vf00w                 iaddiu vi02, vi00, 0xF0
    vu.MaddaBc(XYZW, 9, 0, Fw);
    vu.Iaddiu(2, 0, 0xF0);
    // A78: 01E0E80B 01FC0807  maddw.xyzw vf00, vf29, vf00w                 lq.xyzw vf28, 7(vi01)
    vu.MaddBc(XYZW, 0, 29, 0, Fw);
    vu.Lq(XYZW, 28, 1, 7);
    macA78 = vu.mac;
    // A80: 000002FF 801E03BD  nop                                          sqrt Q, vf30.x
    qA80 = vu.SqrtValue(30, Fx);
    // A88: 000002FF 8000033C  nop                                          nop
    // A90: 000002FF 8000033C  nop                                          nop
    // A98: 01E0E80F 34021000  msubw.xyzw vf00, vf29, vf00w                 fmand vi02, vi02
    vu.MsubBc(XYZW, 0, 29, 0, Fw);
    vu.Fmand(2, 2, macA78);
    macA98 = vu.mac;
    // AA0: 01E2E1BC 01E9080A  mulax.xyzw ACC, vf28, vf02x                  lq.xyzw vf09, 10(vi01)
    vu.MulaBc(XYZW, 28, 2, Fx);
    vu.Lq(XYZW, 9, 1, 10);
    // AA8: 01E208BD 01E1080B  madday.xyzw ACC, vf01, vf02y                 lq.xyzw vf01, 11(vi01)
    vu.MaddaBc(XYZW, 1, 2, Fy);
    vu.Lq(XYZW, 1, 1, 11);
    // AB0: 01E2574A 01E8080C  maddz.xyzw vf29, vf10, vf02z                 lq.xyzw vf08, 12(vi01)
    vu.MaddBc(XYZW, 29, 10, 2, Fz);
    vu.Lq(XYZW, 8, 1, 12);
    vu.q = qA80;
    // AB8: 010007A0 34021000  addq.x vf30, vf00, q                         fmand vi02, vi02
    vu.AddQ(X, 30, 0);
    vu.Fmand(2, 2, macA98);
    // AC0: 01E649BC 01FA080D  mulax.xyzw ACC, vf09, vf06x                  lq.xyzw vf26, 13(vi01)
    vu.MulaBc(XYZW, 9, 6, Fx);
    vu.Lq(XYZW, 26, 1, 13);
    // AC8: 01E608BD 8000033C  madday.xyzw ACC, vf01, vf06y                 nop
    vu.MaddaBc(XYZW, 1, 6, Fy);
    // AD0: 01E640BE 01E8080F  maddaz.xyzw ACC, vf08, vf06z                 lq.xyzw vf08, 15(vi01)
    vu.MaddaBc(XYZW, 8, 6, Fz);
    vu.Lq(XYZW, 8, 1, 15);
    // AD8: 000002FF 807E03BC  nop                                          div Q, vf00.w, vf30.x
    qAD8 = vu.DivValue(0, Fw, 30, Fx);
    // AE0: 01E0D0BF 01E10810  maddaw.xyzw ACC, vf26, vf00w                 lq.xyzw vf01, 16(vi01)
    vu.MaddaBc(XYZW, 26, 0, Fw);
    vu.Lq(XYZW, 1, 1, 16);
    // AE8: 01E0E80B 01E20811  maddw.xyzw vf00, vf29, vf00w                 lq.xyzw vf02, 17(vi01)
    vu.MaddBc(XYZW, 0, 29, 0, Fw);
    vu.Lq(XYZW, 2, 1, 17);
    macAE8 = vu.mac;
    // AF0: 01E0E80F 8000033C  msubw.xyzw vf00, vf29, vf00w                 nop
    vu.MsubBc(XYZW, 0, 29, 0, Fw);
    macAF0 = vu.mac;
    // AF8: 01E341BC 01FA0812  mulax.xyzw ACC, vf08, vf03x                  lq.xyzw vf26, 18(vi01)
    vu.MulaBc(XYZW, 8, 3, Fx);
    vu.Lq(XYZW, 26, 1, 18);
    // B00: 01E308BD 100100F0  madday.xyzw ACC, vf01, vf03y                 iaddiu vi01, vi00, 0xF0
    vu.MaddaBc(XYZW, 1, 3, Fy);
    vu.Iaddiu(1, 0, 0xF0);
    // B08: 01E310BE 34010800  maddaz.xyzw ACC, vf02, vf03z                 fmand vi01, vi01
    vu.MaddaBc(XYZW, 2, 3, Fz);
    vu.Fmand(1, 1, macAE8);
    vu.q = qAD8;
    // B10: 01C0DEDC 34010800  mulq.xyz vf27, vf27, q                       fmand vi01, vi01
    vu.MulQ(XYZ, 27, 27);
    vu.Fmand(1, 1, macAF0);
    // B18: 01E3D74B 8000033C  maddw.xyzw vf29, vf26, vf03w                 nop
    vu.MaddBc(XYZW, 29, 26, 3, Fw);
    // B20: 01E441BC 8000033C  mulax.xyzw ACC, vf08, vf04x                  nop
    vu.MulaBc(XYZW, 8, 4, Fx);
    // B28: 01E408BD 8000033C  madday.xyzw ACC, vf01, vf04y                 nop
    vu.MaddaBc(XYZW, 1, 4, Fy);
    // B30: 01E7DED8 8000033C  mulx.xyzw vf27, vf27, vf07x                  nop
    vu.MulBc(XYZW, 27, 27, 7, Fx);
    // B38: 01E410BE 8000033C  maddaz.xyzw ACC, vf02, vf04z                 nop
    vu.MaddaBc(XYZW, 2, 4, Fz);
    // B40: 01E4D70B 8000033C  maddw.xyzw vf28, vf26, vf04w                 nop
    vu.MaddBc(XYZW, 28, 26, 4, Fw);
    // B48: 01E541BC 8000033C  mulax.xyzw ACC, vf08, vf05x                  nop
    vu.MulaBc(XYZW, 8, 5, Fx);
    // B50: 01FB31AC 8000033C  sub.xyzw vf06, vf06, vf27                    nop
    vu.Sub(XYZW, 6, 6, 27);
    // B58: 01E508BD 8000033C  madday.xyzw ACC, vf01, vf05y                 nop
    vu.MaddaBc(XYZW, 1, 5, Fy);
    // B60: 01E510BE 8000033C  maddaz.xyzw ACC, vf02, vf05z                 nop
    vu.MaddaBc(XYZW, 2, 5, Fz);
    // B68: 01E5D6CB 8000033C  maddw.xyzw vf27, vf26, vf05w                 nop
    vu.MaddBc(XYZW, 27, 26, 5, Fw);
    // B70: 01E641BC 8000033C  mulax.xyzw ACC, vf08, vf06x                  nop
    vu.MulaBc(XYZW, 8, 6, Fx);
    // B78: 01E608BD 8000033C  madday.xyzw ACC, vf01, vf06y                 nop
    vu.MaddaBc(XYZW, 1, 6, Fy);
    // B80: 41E610BE 8000033C  maddaz[e].xyzw ACC, vf02, vf06z              nop
    vu.MaddaBc(XYZW, 2, 6, Fz);
    // B88: 01E6D68B 800110B5  maddw.xyzw vf26, vf26, vf06w                 ior vi02, vi02, vi01
    vu.MaddBc(XYZW, 26, 26, 6, Fw);
    vu.Ior(2, 2, 1);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramCullA08Ranges[] = {{0x141, 0x172}};

// The culling set's program at 0xB90 (Platform::Math::ParticleBlockView (keeping the translation))
void MicroprogramCullB90(Vu0& vu)
{
    u32 macC00;
    u32 macC10;
    u32 qC28;
    u32 macC50;
    u32 macC58;
    u32 qC78;

    // B90: 000002FF 01FD080E  nop                                          lq.xyzw vf29, 14(vi01)
    vu.Lq(XYZW, 29, 1, 14);
    // B98: 000002FF 8000033C  nop                                          nop
    // BA0: 000002FF 01FC0800  nop                                          lq.xyzw vf28, 0(vi01)
    vu.Lq(XYZW, 28, 1, 0);
    // BA8: 000002FF 01FE0801  nop                                          lq.xyzw vf30, 1(vi01)
    vu.Lq(XYZW, 30, 1, 1);
    // BB0: 01E6EF6C 01FB0802  sub.xyzw vf29, vf29, vf06                    lq.xyzw vf27, 2(vi01)
    vu.Sub(XYZW, 29, 29, 6);
    vu.Lq(XYZW, 27, 1, 2);
    // BB8: 01E2E1BC 01FC0803  mulax.xyzw ACC, vf28, vf02x                  lq.xyzw vf28, 3(vi01)
    vu.MulaBc(XYZW, 28, 2, Fx);
    vu.Lq(XYZW, 28, 1, 3);
    // BC0: 01E2F0BD 01FA0804  madday.xyzw ACC, vf30, vf02y                 lq.xyzw vf26, 4(vi01)
    vu.MaddaBc(XYZW, 30, 2, Fy);
    vu.Lq(XYZW, 26, 1, 4);
    // BC8: 01FDEFAA 100300F0  mul.xyzw vf30, vf29, vf29                    iaddiu vi03, vi00, 0xF0
    vu.Mul(XYZW, 30, 29, 29);
    vu.Iaddiu(3, 0, 0xF0);
    // BD0: 01E2D84A 01FB0805  maddz.xyzw vf01, vf27, vf02z                 lq.xyzw vf27, 5(vi01)
    vu.MaddBc(XYZW, 1, 27, 2, Fz);
    vu.Lq(XYZW, 27, 1, 5);
    // BD8: 01E6E1BC 01FC0806  mulax.xyzw ACC, vf28, vf06x                  lq.xyzw vf28, 6(vi01)
    vu.MulaBc(XYZW, 28, 6, Fx);
    vu.Lq(XYZW, 28, 1, 6);
    // BE0: 01E6D0BD 01E30807  madday.xyzw ACC, vf26, vf06y                 lq.xyzw vf03, 7(vi01)
    vu.MaddaBc(XYZW, 26, 6, Fy);
    vu.Lq(XYZW, 3, 1, 7);
    // BE8: 011EF781 100200F0  addy.x vf30, vf30, vf30y                     iaddiu vi02, vi00, 0xF0
    vu.AddBc(X, 30, 30, 30, Fy);
    vu.Iaddiu(2, 0, 0xF0);
    // BF0: 01E6D8BE 01FA0808  maddaz.xyzw ACC, vf27, vf06z                 lq.xyzw vf26, 8(vi01)
    vu.MaddaBc(XYZW, 27, 6, Fz);
    vu.Lq(XYZW, 26, 1, 8);
    // BF8: 01E0E0BF 01FB080C  maddaw.xyzw ACC, vf28, vf00w                 lq.xyzw vf27, 12(vi01)
    vu.MaddaBc(XYZW, 28, 0, Fw);
    vu.Lq(XYZW, 27, 1, 12);
    // C00: 01E0080B 01E50809  maddw.xyzw vf00, vf01, vf00w                 lq.xyzw vf05, 9(vi01)
    vu.MaddBc(XYZW, 0, 1, 0, Fw);
    vu.Lq(XYZW, 5, 1, 9);
    macC00 = vu.mac;
    // C08: 011EF782 01FC080D  addz.x vf30, vf30, vf30z                     lq.xyzw vf28, 13(vi01)
    vu.AddBc(X, 30, 30, 30, Fz);
    vu.Lq(XYZW, 28, 1, 13);
    // C10: 01E0080F 01E4080A  msubw.xyzw vf00, vf01, vf00w                 lq.xyzw vf04, 10(vi01)
    vu.MsubBc(XYZW, 0, 1, 0, Fw);
    vu.Lq(XYZW, 4, 1, 10);
    macC10 = vu.mac;
    // C18: 01E219BC 01E1080B  mulax.xyzw ACC, vf03, vf02x                  lq.xyzw vf01, 11(vi01)
    vu.MulaBc(XYZW, 3, 2, Fx);
    vu.Lq(XYZW, 1, 1, 11);
    // C20: 01E2D0BD 34021000  madday.xyzw ACC, vf26, vf02y                 fmand vi02, vi02
    vu.MaddaBc(XYZW, 26, 2, Fy);
    vu.Fmand(2, 2, macC00);
    // C28: 01E2288A 801E03BD  maddz.xyzw vf02, vf05, vf02z                 sqrt Q, vf30.x
    vu.MaddBc(XYZW, 2, 5, 2, Fz);
    qC28 = vu.SqrtValue(30, Fx);
    // C30: 01E621BC 34021000  mulax.xyzw ACC, vf04, vf06x                  fmand vi02, vi02
    vu.MulaBc(XYZW, 4, 6, Fx);
    vu.Fmand(2, 2, macC10);
    // C38: 01E608BD 8000033C  madday.xyzw ACC, vf01, vf06y                 nop
    vu.MaddaBc(XYZW, 1, 6, Fy);
    // C40: 01E6D8BE 01FB0811  maddaz.xyzw ACC, vf27, vf06z                 lq.xyzw vf27, 17(vi01)
    vu.MaddaBc(XYZW, 27, 6, Fz);
    vu.Lq(XYZW, 27, 1, 17);
    // C48: 01E0E0BF 01FC0810  maddaw.xyzw ACC, vf28, vf00w                 lq.xyzw vf28, 16(vi01)
    vu.MaddaBc(XYZW, 28, 0, Fw);
    vu.Lq(XYZW, 28, 1, 16);
    // C50: 01E0120B 8000033C  maddw.xyzw vf08, vf02, vf00w                 nop
    vu.MaddBc(XYZW, 8, 2, 0, Fw);
    macC50 = vu.mac;
    // C58: 01E0100F 01E20812  msubw.xyzw vf00, vf02, vf00w                 lq.xyzw vf02, 18(vi01)
    vu.MsubBc(XYZW, 0, 2, 0, Fw);
    vu.Lq(XYZW, 2, 1, 18);
    macC58 = vu.mac;
    vu.q = qC28;
    // C60: 010007A0 800003BF  addq.x vf30, vf00, q                         waitq
    vu.AddQ(X, 30, 0);
    // C68: 01E041FD 34031800  abs.xyzw vf00, vf08                          fmand vi03, vi03
    vu.Abs(XYZW, 0, 8);
    vu.Fmand(3, 3, macC50);
    // C70: 000002FF 34031800  nop                                          fmand vi03, vi03
    vu.Fmand(3, 3, macC58);
    // C78: 000002FF 807E03BC  nop                                          div Q, vf00.w, vf30.x
    qC78 = vu.DivValue(0, Fw, 30, Fx);
    vu.q = qC78;
    // C80: 01C0EF5C 800003BF  mulq.xyz vf29, vf29, q                       waitq
    vu.MulQ(XYZ, 29, 29);
    // C88: 01C7E998 01FD080F  mulx.xyz vf06, vf29, vf07x                   lq.xyzw vf29, 15(vi01)
    vu.MulBc(XYZ, 6, 29, 7, Fx);
    vu.Lq(XYZW, 29, 1, 15);
    // C90: 01E6E9BC 8000033C  mulax.xyzw ACC, vf29, vf06x                  nop
    vu.MulaBc(XYZW, 29, 6, Fx);
    // C98: 01E6E0BD 8000033C  madday.xyzw ACC, vf28, vf06y                 nop
    vu.MaddaBc(XYZW, 28, 6, Fy);
    // CA0: 41E6D8BE 8000033C  maddaz[e].xyzw ACC, vf27, vf06z              nop
    vu.MaddaBc(XYZW, 27, 6, Fz);
    // CA8: 01E0168B 800310B5  maddw.xyzw vf26, vf02, vf00w                 ior vi02, vi02, vi03
    vu.MaddBc(XYZW, 26, 2, 0, Fw);
    vu.Ior(2, 2, 3);
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramCullB90Ranges[] = {{0x172, 0x196}};

// The culling set's program at 0xCB0 (Platform::Math::ViewDistance)
void MicroprogramCullCB0(Vu0& vu)
{
    u32 qCD8;

    // CB0: 000002FF 01FE080E  nop                                          lq.xyzw vf30, 14(vi01)
    vu.Lq(XYZW, 30, 1, 14);
    // CB8: 01E1F7AC 8000033C  sub.xyzw vf30, vf30, vf01                    nop
    vu.Sub(XYZW, 30, 30, 1);
    // CC0: 01FEF7AA 8000033C  mul.xyzw vf30, vf30, vf30                    nop
    vu.Mul(XYZW, 30, 30, 30);
    // CC8: 011EF781 8000033C  addy.x vf30, vf30, vf30y                     nop
    vu.AddBc(X, 30, 30, 30, Fy);
    // CD0: 011EF782 8000033C  addz.x vf30, vf30, vf30z                     nop
    vu.AddBc(X, 30, 30, 30, Fz);
    // CD8: 000002FF 801E03BD  nop                                          sqrt Q, vf30.x
    qCD8 = vu.SqrtValue(30, Fx);
    vu.q = qCD8;
    // CE0: 010007A0 800003BF  addq.x vf30, vf00, q                         waitq
    vu.AddQ(X, 30, 0);
    // CE8: 400002FF 8000033C  nop[e]                                       nop
    // CF0: 000002FF 8000033C  nop                                          nop
    // The end: what's still in the pipelines written
    return;
}

const MicroInstructionRange MicroprogramCullCB0Ranges[] = {{0x196, 0x19F}};

const MicroInstructionRange StdLoads[] = {{0x0, 0x100}, {0x100, 0x1B3}};
const MicroInstructionRange CullLoads[] = {{0x0, 0x100}, {0x100, 0x19F}};
const MicroInstructionRange DecalLoads[] = {{0x0, 0xA4}};

const Microprogram s_Programs[] = {
    {0x0F0, 0xE, MicroprogramStd0F0Ranges, 1, MicroprogramStd0F0},
    {0x1C0, 0x2, MicroprogramStd1C0Ranges, 1, MicroprogramStd1C0},
    {0x548, 0x2, MicroprogramStd548Ranges, 1, MicroprogramStd548},
    {0x668, 0x2, MicroprogramStd668Ranges, 1, MicroprogramStd668},
    {0x790, 0x2, MicroprogramStd790Ranges, 1, MicroprogramStd790},
    {0x818, 0x2, MicroprogramStd818Ranges, 1, MicroprogramStd818},
    {0xA70, 0x2, MicroprogramStdA70Ranges, 1, MicroprogramStdA70},
    {0xAB8, 0x2, MicroprogramStdAB8Ranges, 1, MicroprogramStdAB8},
    {0xA08, 0x4, MicroprogramCullA08Ranges, 1, MicroprogramCullA08},
    {0xB90, 0x4, MicroprogramCullB90Ranges, 1, MicroprogramCullB90},
    {0xCB0, 0x4, MicroprogramCullCB0Ranges, 1, MicroprogramCullCB0},
};

const MicrocodeSetLoads s_SetLoads[] = {
    {1, StdLoads, 2},
    {2, CullLoads, 2},
    {3, DecalLoads, 1},
};
}

const Microprogram* FindMicroprogram(u32 address)
{
    for (const Microprogram& program : s_Programs)
    {
        if (program.address == address)
        {
            return &program;
        }
    }

    return nullptr;
}

const MicrocodeSetLoads* MicrocodeSetLoadsOf(u32 set)
{
    for (const MicrocodeSetLoads& loads : s_SetLoads)
    {
        if (loads.set == set)
        {
            return &loads;
        }
    }

    return nullptr;
}

const Microprogram* Microprograms(u32* count)
{
    *count = sizeof(s_Programs) / sizeof(s_Programs[0]);
    return s_Programs;
}
}
