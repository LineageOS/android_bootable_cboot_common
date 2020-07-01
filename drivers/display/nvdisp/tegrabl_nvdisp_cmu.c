/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVDISP

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_nvdisp.h>
#include <tegrabl_nvdisp_local.h>
#include <tegrabl_io.h>
#include <ardisplay.h>
#include <tegrabl_drf.h>

#define NVDISP_LUT_SIZE 1025

uint32_t default_srgb_lut[] = {
	0x00006000,  0x000060ce,  0x0000619d,  0x0000626c,  0x0000632d,
	0x000063d4,  0x00006469,  0x000064f0,  0x0000656b,  0x000065df,
	0x0000664a,  0x000066b0,  0x00006711,  0x0000676d,  0x000067c4,
	0x00006819,  0x0000686a,  0x000068b8,  0x00006904,  0x0000694d,
	0x00006994,  0x000069d8,  0x00006a1b,  0x00006a5d,  0x00006a9c,
	0x00006ada,  0x00006b17,  0x00006b52,  0x00006b8c,  0x00006bc5,
	0x00006bfd,  0x00006c33,  0x00006c69,  0x00006c9e,  0x00006cd1,
	0x00006d04,  0x00006d36,  0x00006d67,  0x00006d98,  0x00006dc7,
	0x00006df6,  0x00006e25,  0x00006e52,  0x00006e7f,  0x00006eac,
	0x00006ed7,  0x00006f03,  0x00006f2d,  0x00006f58,  0x00006f81,
	0x00006faa,  0x00006fd3,  0x00006ffb,  0x00007023,  0x0000704b,
	0x00007071,  0x00007098,  0x000070be,  0x000070e4,  0x00007109,
	0x0000712e,  0x00007153,  0x00007177,  0x0000719b,  0x000071bf,
	0x000071e2,  0x00007205,  0x00007227,  0x0000724a,  0x0000726c,
	0x0000728e,  0x000072af,  0x000072d0,  0x000072f1,  0x00007312,
	0x00007333,  0x00007353,  0x00007373,  0x00007392,  0x000073b2,
	0x000073d1,  0x000073f0,  0x0000740f,  0x0000742d,  0x0000744c,
	0x0000746a,  0x00007488,  0x000074a6,  0x000074c3,  0x000074e0,
	0x000074fe,  0x0000751b,  0x00007537,  0x00007554,  0x00007570,
	0x0000758d,  0x000075a9,  0x000075c4,  0x000075e0,  0x000075fc,
	0x00007617,  0x00007632,  0x0000764d,  0x00007668,  0x00007683,
	0x0000769e,  0x000076b8,  0x000076d3,  0x000076ed,  0x00007707,
	0x00007721,  0x0000773b,  0x00007754,  0x0000776e,  0x00007787,
	0x000077a0,  0x000077b9,  0x000077d2,  0x000077eb,  0x00007804,
	0x0000781d,  0x00007835,  0x0000784e,  0x00007866,  0x0000787e,
	0x00007896,  0x000078ae,  0x000078c6,  0x000078dd,  0x000078f5,
	0x0000790d,  0x00007924,  0x0000793b,  0x00007952,  0x0000796a,
	0x00007981,  0x00007997,  0x000079ae,  0x000079c5,  0x000079db,
	0x000079f2,  0x00007a08,  0x00007a1f,  0x00007a35,  0x00007a4b,
	0x00007a61,  0x00007a77,  0x00007a8d,  0x00007aa3,  0x00007ab8,
	0x00007ace,  0x00007ae3,  0x00007af9,  0x00007b0e,  0x00007b24,
	0x00007b39,  0x00007b4e,  0x00007b63,  0x00007b78,  0x00007b8d,
	0x00007ba2,  0x00007bb6,  0x00007bcb,  0x00007be0,  0x00007bf4,
	0x00007c08,  0x00007c1d,  0x00007c31,  0x00007c45,  0x00007c59,
	0x00007c6e,  0x00007c82,  0x00007c96,  0x00007ca9,  0x00007cbd,
	0x00007cd1,  0x00007ce5,  0x00007cf8,  0x00007d0c,  0x00007d1f,
	0x00007d33,  0x00007d46,  0x00007d59,  0x00007d6d,  0x00007d80,
	0x00007d93,  0x00007da6,  0x00007db9,  0x00007dcc,  0x00007ddf,
	0x00007df2,  0x00007e04,  0x00007e17,  0x00007e2a,  0x00007e3c,
	0x00007e4f,  0x00007e61,  0x00007e74,  0x00007e86,  0x00007e98,
	0x00007eab,  0x00007ebd,  0x00007ecf,  0x00007ee1,  0x00007ef3,
	0x00007f05,  0x00007f17,  0x00007f29,  0x00007f3b,  0x00007f4d,
	0x00007f5e,  0x00007f70,  0x00007f82,  0x00007f93,  0x00007fa5,
	0x00007fb6,  0x00007fc8,  0x00007fd9,  0x00007feb,  0x00007ffc,
	0x0000800d,  0x0000801e,  0x00008030,  0x00008041,  0x00008052,
	0x00008063,  0x00008074,  0x00008085,  0x00008096,  0x000080a7,
	0x000080b7,  0x000080c8,  0x000080d9,  0x000080ea,  0x000080fa,
	0x0000810b,  0x0000811c,  0x0000812c,  0x0000813d,  0x0000814d,
	0x0000815d,  0x0000816e,  0x0000817e,  0x0000818e,  0x0000819f,
	0x000081af,  0x000081bf,  0x000081cf,  0x000081df,  0x000081ef,
	0x000081ff,  0x0000820f,  0x0000821f,  0x0000822f,  0x0000823f,
	0x0000824f,  0x0000825f,  0x0000826f,  0x0000827e,  0x0000828e,
	0x0000829e,  0x000082ad,  0x000082bd,  0x000082cc,  0x000082dc,
	0x000082eb,  0x000082fb,  0x0000830a,  0x0000831a,  0x00008329,
	0x00008338,  0x00008348,  0x00008357,  0x00008366,  0x00008375,
	0x00008385,  0x00008394,  0x000083a3,  0x000083b2,  0x000083c1,
	0x000083d0,  0x000083df,  0x000083ee,  0x000083fd,  0x0000840c,
	0x0000841a,  0x00008429,  0x00008438,  0x00008447,  0x00008455,
	0x00008464,  0x00008473,  0x00008481,  0x00008490,  0x0000849f,
	0x000084ad,  0x000084bc,  0x000084ca,  0x000084d9,  0x000084e7,
	0x000084f5,  0x00008504,  0x00008512,  0x00008521,  0x0000852f,
	0x0000853d,  0x0000854b,  0x0000855a,  0x00008568,  0x00008576,
	0x00008584,  0x00008592,  0x000085a0,  0x000085ae,  0x000085bc,
	0x000085ca,  0x000085d8,  0x000085e6,  0x000085f4,  0x00008602,
	0x00008610,  0x0000861e,  0x0000862c,  0x00008639,  0x00008647,
	0x00008655,  0x00008663,  0x00008670,  0x0000867e,  0x0000868c,
	0x00008699,  0x000086a7,  0x000086b5,  0x000086c2,  0x000086d0,
	0x000086dd,  0x000086eb,  0x000086f8,  0x00008705,  0x00008713,
	0x00008720,  0x0000872e,  0x0000873b,  0x00008748,  0x00008756,
	0x00008763,  0x00008770,  0x0000877d,  0x0000878b,  0x00008798,
	0x000087a5,  0x000087b2,  0x000087bf,  0x000087cc,  0x000087d9,
	0x000087e6,  0x000087f3,  0x00008801,  0x0000880e,  0x0000881a,
	0x00008827,  0x00008834,  0x00008841,  0x0000884e,  0x0000885b,
	0x00008868,  0x00008875,  0x00008882,  0x0000888e,  0x0000889b,
	0x000088a8,  0x000088b5,  0x000088c1,  0x000088ce,  0x000088db,
	0x000088e7,  0x000088f4,  0x00008900,  0x0000890d,  0x0000891a,
	0x00008926,  0x00008933,  0x0000893f,  0x0000894c,  0x00008958,
	0x00008965,  0x00008971,  0x0000897d,  0x0000898a,  0x00008996,
	0x000089a3,  0x000089af,  0x000089bb,  0x000089c8,  0x000089d4,
	0x000089e0,  0x000089ec,  0x000089f9,  0x00008a05,  0x00008a11,
	0x00008a1d,  0x00008a29,  0x00008a36,  0x00008a42,  0x00008a4e,
	0x00008a5a,  0x00008a66,  0x00008a72,  0x00008a7e,  0x00008a8a,
	0x00008a96,  0x00008aa2,  0x00008aae,  0x00008aba,  0x00008ac6,
	0x00008ad2,  0x00008ade,  0x00008aea,  0x00008af5,  0x00008b01,
	0x00008b0d,  0x00008b19,  0x00008b25,  0x00008b31,  0x00008b3c,
	0x00008b48,  0x00008b54,  0x00008b60,  0x00008b6b,  0x00008b77,
	0x00008b83,  0x00008b8e,  0x00008b9a,  0x00008ba6,  0x00008bb1,
	0x00008bbd,  0x00008bc8,  0x00008bd4,  0x00008bdf,  0x00008beb,
	0x00008bf6,  0x00008c02,  0x00008c0d,  0x00008c19,  0x00008c24,
	0x00008c30,  0x00008c3b,  0x00008c47,  0x00008c52,  0x00008c5d,
	0x00008c69,  0x00008c74,  0x00008c80,  0x00008c8b,  0x00008c96,
	0x00008ca1,  0x00008cad,  0x00008cb8,  0x00008cc3,  0x00008ccf,
	0x00008cda,  0x00008ce5,  0x00008cf0,  0x00008cfb,  0x00008d06,
	0x00008d12,  0x00008d1d,  0x00008d28,  0x00008d33,  0x00008d3e,
	0x00008d49,  0x00008d54,  0x00008d5f,  0x00008d6a,  0x00008d75,
	0x00008d80,  0x00008d8b,  0x00008d96,  0x00008da1,  0x00008dac,
	0x00008db7,  0x00008dc2,  0x00008dcd,  0x00008dd8,  0x00008de3,
	0x00008dee,  0x00008df9,  0x00008e04,  0x00008e0e,  0x00008e19,
	0x00008e24,  0x00008e2f,  0x00008e3a,  0x00008e44,  0x00008e4f,
	0x00008e5a,  0x00008e65,  0x00008e6f,  0x00008e7a,  0x00008e85,
	0x00008e90,  0x00008e9a,  0x00008ea5,  0x00008eb0,  0x00008eba,
	0x00008ec5,  0x00008ecf,  0x00008eda,  0x00008ee5,  0x00008eef,
	0x00008efa,  0x00008f04,  0x00008f0f,  0x00008f19,  0x00008f24,
	0x00008f2e,  0x00008f39,  0x00008f43,  0x00008f4e,  0x00008f58,
	0x00008f63,  0x00008f6d,  0x00008f78,  0x00008f82,  0x00008f8c,
	0x00008f97,  0x00008fa1,  0x00008fac,  0x00008fb6,  0x00008fc0,
	0x00008fcb,  0x00008fd5,  0x00008fdf,  0x00008fea,  0x00008ff4,
	0x00008ffe,  0x00009008,  0x00009013,  0x0000901d,  0x00009027,
	0x00009031,  0x0000903c,  0x00009046,  0x00009050,  0x0000905a,
	0x00009064,  0x0000906e,  0x00009079,  0x00009083,  0x0000908d,
	0x00009097,  0x000090a1,  0x000090ab,  0x000090b5,  0x000090bf,
	0x000090c9,  0x000090d3,  0x000090dd,  0x000090e7,  0x000090f1,
	0x000090fb,  0x00009105,  0x0000910f,  0x00009119,  0x00009123,
	0x0000912d,  0x00009137,  0x00009141,  0x0000914b,  0x00009155,
	0x0000915f,  0x00009169,  0x00009173,  0x0000917d,  0x00009186,
	0x00009190,  0x0000919a,  0x000091a4,  0x000091ae,  0x000091b8,
	0x000091c1,  0x000091cb,  0x000091d5,  0x000091df,  0x000091e9,
	0x000091f2,  0x000091fc,  0x00009206,  0x00009210,  0x00009219,
	0x00009223,  0x0000922d,  0x00009236,  0x00009240,  0x0000924a,
	0x00009253,  0x0000925d,  0x00009267,  0x00009270,  0x0000927a,
	0x00009283,  0x0000928d,  0x00009297,  0x000092a0,  0x000092aa,
	0x000092b3,  0x000092bd,  0x000092c6,  0x000092d0,  0x000092da,
	0x000092e3,  0x000092ed,  0x000092f6,  0x00009300,  0x00009309,
	0x00009313,  0x0000931c,  0x00009325,  0x0000932f,  0x00009338,
	0x00009342,  0x0000934b,  0x00009355,  0x0000935e,  0x00009367,
	0x00009371,  0x0000937a,  0x00009384,  0x0000938d,  0x00009396,
	0x000093a0,  0x000093a9,  0x000093b2,  0x000093bc,  0x000093c5,
	0x000093ce,  0x000093d7,  0x000093e1,  0x000093ea,  0x000093f3,
	0x000093fc,  0x00009406,  0x0000940f,  0x00009418,  0x00009421,
	0x0000942b,  0x00009434,  0x0000943d,  0x00009446,  0x0000944f,
	0x00009459,  0x00009462,  0x0000946b,  0x00009474,  0x0000947d,
	0x00009486,  0x0000948f,  0x00009499,  0x000094a2,  0x000094ab,
	0x000094b4,  0x000094bd,  0x000094c6,  0x000094cf,  0x000094d8,
	0x000094e1,  0x000094ea,  0x000094f3,  0x000094fc,  0x00009505,
	0x0000950e,  0x00009517,  0x00009520,  0x00009529,  0x00009532,
	0x0000953b,  0x00009544,  0x0000954d,  0x00009556,  0x0000955f,
	0x00009568,  0x00009571,  0x0000957a,  0x00009583,  0x0000958c,
	0x00009595,  0x0000959d,  0x000095a6,  0x000095af,  0x000095b8,
	0x000095c1,  0x000095ca,  0x000095d3,  0x000095db,  0x000095e4,
	0x000095ed,  0x000095f6,  0x000095ff,  0x00009608,  0x00009610,
	0x00009619,  0x00009622,  0x0000962b,  0x00009633,  0x0000963c,
	0x00009645,  0x0000964e,  0x00009656,  0x0000965f,  0x00009668,
	0x00009671,  0x00009679,  0x00009682,  0x0000968b,  0x00009693,
	0x0000969c,  0x000096a5,  0x000096ad,  0x000096b6,  0x000096bf,
	0x000096c7,  0x000096d0,  0x000096d9,  0x000096e1,  0x000096ea,
	0x000096f2,  0x000096fb,  0x00009704,  0x0000970c,  0x00009715,
	0x0000971d,  0x00009726,  0x0000972e,  0x00009737,  0x00009740,
	0x00009748,  0x00009751,  0x00009759,  0x00009762,  0x0000976a,
	0x00009773,  0x0000977b,  0x00009784,  0x0000978c,  0x00009795,
	0x0000979d,  0x000097a6,  0x000097ae,  0x000097b6,  0x000097bf,
	0x000097c7,  0x000097d0,  0x000097d8,  0x000097e1,  0x000097e9,
	0x000097f1,  0x000097fa,  0x00009802,  0x0000980b,  0x00009813,
	0x0000981b,  0x00009824,  0x0000982c,  0x00009834,  0x0000983d,
	0x00009845,  0x0000984d,  0x00009856,  0x0000985e,  0x00009866,
	0x0000986f,  0x00009877,  0x0000987f,  0x00009888,  0x00009890,
	0x00009898,  0x000098a0,  0x000098a9,  0x000098b1,  0x000098b9,
	0x000098c1,  0x000098ca,  0x000098d2,  0x000098da,  0x000098e2,
	0x000098eb,  0x000098f3,  0x000098fb,  0x00009903,  0x0000990b,
	0x00009914,  0x0000991c,  0x00009924,  0x0000992c,  0x00009934,
	0x0000993c,  0x00009945,  0x0000994d,  0x00009955,  0x0000995d,
	0x00009965,  0x0000996d,  0x00009975,  0x0000997d,  0x00009986,
	0x0000998e,  0x00009996,  0x0000999e,  0x000099a6,  0x000099ae,
	0x000099b6,  0x000099be,  0x000099c6,  0x000099ce,  0x000099d6,
	0x000099de,  0x000099e6,  0x000099ee,  0x000099f6,  0x000099fe,
	0x00009a06,  0x00009a0e,  0x00009a16,  0x00009a1e,  0x00009a26,
	0x00009a2e,  0x00009a36,  0x00009a3e,  0x00009a46,  0x00009a4e,
	0x00009a56,  0x00009a5e,  0x00009a66,  0x00009a6e,  0x00009a76,
	0x00009a7e,  0x00009a86,  0x00009a8e,  0x00009a96,  0x00009a9d,
	0x00009aa5,  0x00009aad,  0x00009ab5,  0x00009abd,  0x00009ac5,
	0x00009acd,  0x00009ad5,  0x00009adc,  0x00009ae4,  0x00009aec,
	0x00009af4,  0x00009afc,  0x00009b04,  0x00009b0c,  0x00009b13,
	0x00009b1b,  0x00009b23,  0x00009b2b,  0x00009b33,  0x00009b3a,
	0x00009b42,  0x00009b4a,  0x00009b52,  0x00009b59,  0x00009b61,
	0x00009b69,  0x00009b71,  0x00009b79,  0x00009b80,  0x00009b88,
	0x00009b90,  0x00009b97,  0x00009b9f,  0x00009ba7,  0x00009baf,
	0x00009bb6,  0x00009bbe,  0x00009bc6,  0x00009bcd,  0x00009bd5,
	0x00009bdd,  0x00009be5,  0x00009bec,  0x00009bf4,  0x00009bfc,
	0x00009c03,  0x00009c0b,  0x00009c12,  0x00009c1a,  0x00009c22,
	0x00009c29,  0x00009c31,  0x00009c39,  0x00009c40,  0x00009c48,
	0x00009c50,  0x00009c57,  0x00009c5f,  0x00009c66,  0x00009c6e,
	0x00009c75,  0x00009c7d,  0x00009c85,  0x00009c8c,  0x00009c94,
	0x00009c9b,  0x00009ca3,  0x00009caa,  0x00009cb2,  0x00009cba,
	0x00009cc1,  0x00009cc9,  0x00009cd0,  0x00009cd8,  0x00009cdf,
	0x00009ce7,  0x00009cee,  0x00009cf6,  0x00009cfd,  0x00009d05,
	0x00009d0c,  0x00009d14,  0x00009d1b,  0x00009d23,  0x00009d2a,
	0x00009d32,  0x00009d39,  0x00009d40,  0x00009d48,  0x00009d4f,
	0x00009d57,  0x00009d5e,  0x00009d66,  0x00009d6d,  0x00009d75,
	0x00009d7c,  0x00009d83,  0x00009d8b,  0x00009d92,  0x00009d9a,
	0x00009da1,  0x00009da8,  0x00009db0,  0x00009db7,  0x00009dbe,
	0x00009dc6,  0x00009dcd,  0x00009dd5,  0x00009ddc,  0x00009de3,
	0x00009deb,  0x00009df2,  0x00009df9,  0x00009e01,  0x00009e08,
	0x00009e0f,  0x00009e17,  0x00009e1e,  0x00009e25,  0x00009e2d,
	0x00009e34,  0x00009e3b,  0x00009e43,  0x00009e4a,  0x00009e51,
	0x00009e58,  0x00009e60,  0x00009e67,  0x00009e6e,  0x00009e75,
	0x00009e7d,  0x00009e84,  0x00009e8b,  0x00009e92,  0x00009e9a,
	0x00009ea1,  0x00009ea8,  0x00009eaf,  0x00009eb7,  0x00009ebe,
	0x00009ec5,  0x00009ecc,  0x00009ed4,  0x00009edb,  0x00009ee2,
	0x00009ee9,  0x00009ef0,  0x00009ef7,  0x00009eff,  0x00009f06,
	0x00009f0d,  0x00009f14,  0x00009f1b,  0x00009f23,  0x00009f2a,
	0x00009f31,  0x00009f38,  0x00009f3f,  0x00009f46,  0x00009f4d,
	0x00009f55,  0x00009f5c,  0x00009f63,  0x00009f6a,  0x00009f71,
	0x00009f78,  0x00009f7f,  0x00009f86,  0x00009f8d,  0x00009f95,
	0x00009f9c,  0x00009fa3,  0x00009faa,  0x00009fb1,  0x00009fb8,
	0x00009fbf,  0x00009fc6,  0x00009fcd,  0x00009fd4,  0x00009fdb,
	0x00009fe2,  0x00009fe9,  0x00009ff0,  0x00009ff7,  0x00009ffe,
};

void nvdisp_cmu_init_defaults(struct nvdisp_cmu *cmu)
{
	uint32_t i;
	uint64_t r = 0;

	for (i = 0; i < NVDISP_LUT_SIZE; i++) {
		r = default_srgb_lut[i];
		cmu->rgb[i] = (r << 32) | (r << 16) | r;
	}
}

void nvdisp_cmu_set(struct tegrabl_nvdisp *nvdisp, struct nvdisp_cmu *cmu)
{
	uint32_t val;
	dma_addr_t cmu_base_addr;

	pr_debug("%s: entry\n", __func__);

	cmu_base_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_NVDISPLAY0_HEAD,
		nvdisp->instance, (void *)(cmu), sizeof(struct nvdisp_cmu),
		TEGRABL_DMA_TO_DEVICE);

	nvdisp->cmu_base_addr = (uintptr_t)cmu_base_addr;

	nvdisp_writel(nvdisp, DISP_COREPVT_HEAD_SET_OUTPUT_LUT_BASE,
				  U64_TO_U32_LO(cmu_base_addr));

	nvdisp_writel(nvdisp, DISP_COREPVT_HEAD_SET_OUTPUT_LUT_BASE_HI,
				  U64_TO_U32_HI(cmu_base_addr));

	val = nvdisp_readl(nvdisp, DISP_CORE_HEAD_SET_CONTROL_OUTPUT_LUT);
	val = NV_FLD_SET_DRF_DEF(DC_DISP, CORE_HEAD_SET_CONTROL_OUTPUT_LUT,
							 OUTPUT_MODE, INTERPOLATE, val);
	val = NV_FLD_SET_DRF_DEF(DC_DISP, CORE_HEAD_SET_CONTROL_OUTPUT_LUT,
							 SIZE, SIZE_1025, val);
	nvdisp_writel(nvdisp, DISP_CORE_HEAD_SET_CONTROL_OUTPUT_LUT, val);

	val = nvdisp_readl(nvdisp, DISP_DISP_COLOR_CONTROL);
	if (nvdisp->flags & NVDISP_FLAG_CMU_ENABLE)
		val = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_COLOR_CONTROL, CMU_ENABLE,
								 ENABLE, val);
	else
		val = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_COLOR_CONTROL, CMU_ENABLE,
								 DISABLE, val);
	nvdisp_writel(nvdisp, DISP_DISP_COLOR_CONTROL, val);

	pr_debug("%s: exit\n", __func__);
}
