/**
 * GS1 barcode encoder application
 *
 * @author Copyright (c) 2000-2020 GS1 AISBL.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "enc-private.h"
#include "cc.h"
#include "driver.h"
#include "ean.h"


#define EAN13_ELMNTS	61	// includes qz's
#define EAN13_W		109 	// includes 7X quiet zones
#define EAN13_H		74	// total ht in x

#define EAN13_L_PAD 3		// EAN-13 7-X qz - CCA-2 3 offset
#define EAN13_R_PAD 5		// EAN-13 WIDTH - MAX_WIDTH - EAN13_L_PAD

// call with str = 13-digit primary with check digit = 0
static bool EAN13enc(uint8_t str[], uint8_t pattern[] ) {

	static const uint16_t upcTblA[10] = {	0x3211, 0x2221, 0x2122, 0x1411, 0x1132,
					0x1231, 0x1114, 0x1312, 0x1213, 0x3112 };
	static const uint16_t upcTblB[10] = {	0x1123, 0x1222, 0x2212, 0x1141, 0x2311,
					0x1321, 0x4111, 0x2131, 0x3121, 0x2113 };
	static const uint16_t abArr[10] = { 0 /*0x07*/,0x0B,0x0D,0x0E,0x13,0x19,0x1C,0x15,0x16,0x1A };
	static const uint8_t lGuard[4] = { 7,1,1,1 };
	static const uint8_t center[5] = { 1,1,1,1,1 };
	static const uint8_t rGuard[4] = { 1,1,1,7 };

	int i, j, abMask, bars, sNdx, pNdx, abBits;

	// calculate UPC parity
	for (j = 0, i = 0; i < 12; i += 2) {
		j += str[i] - '0';
		j += (str[i+1] - '0') * 3;
	}
	j = (j%10);
	if (j > 0) {
		j = 10 - j;
	}
	str[12] = (uint8_t)(j + '0');

	sNdx = 1;
	pNdx = 0;
	for (i = 0; i < 4; i++) {
		pattern[pNdx++] = lGuard[i];
	}
	abBits = abArr[str[0]-'0'];
	for (abMask = 0x20, i = 0; i < 6; abMask >>= 1, i++) {
		if ((abBits & abMask) == 0) {
			bars = upcTblA[str[sNdx++]-'0'];
		}
		else {
			bars = upcTblB[str[sNdx++]-'0'];
		}
		for (j = 12; j >= 0; j -= 4) {
			pattern[pNdx++] = (bars >> j) & 0xf;
		}
	}
	for (i = 0; i < 5; i++) {
		pattern[pNdx++] = center[i];
	}
	for (i = 0; i < 6; i++) {
		bars = upcTblA[str[sNdx++]-'0'];
		for (j = 12; j >= 0; j -= 4) {
			pattern[pNdx++] = (bars >> j) & 0xf;
		}
	}
	for (i = 0; i < 4; i++) {
		pattern[pNdx++] = rGuard[i];
	}
	return(true);
}

void gs1_EAN13(gs1_encoder *ctx) {

	struct sPrints prints;
	struct sPrints sepPrnt;

	uint8_t linPattern[EAN13_ELMNTS];
	uint8_t sepPat1[5] = { 7,1,EAN13_W-16,1,7 }; // separator pattern 1
	uint8_t sepPat2[5] = { 6,1,EAN13_W-14,1,6 }; // separator pattern 2

	uint8_t (*ccPattern)[CCB4_ELMNTS] = ctx->ccPattern;

	char primaryStr[14+1];
	char tempStr[28+1];

	int i;
	int rows, ccFlag;
	char *ccStr;

	ccStr = strchr(ctx->dataStr, '|');
	if (ccStr == NULL) ccFlag = false;
	else {
		ccFlag = true;
		ccStr[0] = '\0'; // separate primary data
		ccStr++; // point to secondary data
	}

	if (strlen(ctx->dataStr) > 12) {
		strcpy(ctx->errMsg, "primary data exceeds 12 digits");
		ctx->errFlag = true;
		return;
	}

	strcpy(tempStr, "000000000000");
	strcat(tempStr, ctx->dataStr);
	strcat(tempStr, "0"); // check digit = 0 for now
	strcpy(primaryStr, tempStr + strlen(tempStr) - 13);

	if (!EAN13enc((uint8_t*)primaryStr, linPattern) || ctx->errFlag) return;
#if PRNT
	printf("\n%s", primaryStr);
	printf("\n");
	for (i = 0; i < EAN13_ELMNTS; i++) {
		printf("%d", linPattern[i]);
	}
	printf("\n");
#endif
	ctx->line1 = true; // so first line is not Y undercut
	// init most likely prints values
	prints.elmCnt = EAN13_ELMNTS;
	prints.pattern = linPattern;
	prints.guards = false;
	prints.height = ctx->pixMult*EAN13_H;
	prints.leftPad = 0;
	prints.rightPad = 0;
	prints.whtFirst = true;
	prints.reverse = false;
	// init most likely separator values
	sepPrnt.elmCnt = 5;
	sepPrnt.pattern = sepPat1;
	sepPrnt.guards = false;
	sepPrnt.height = ctx->pixMult*2;
	sepPrnt.leftPad = 0;
	sepPrnt.rightPad = 0;
	sepPrnt.whtFirst = true;
	sepPrnt.reverse = false;
	if (ccFlag) {
		if (!((rows = gs1_CC4enc(ctx, (uint8_t*)ccStr, ccPattern)) > 0) || ctx->errFlag) return;
#if PRNT
		{
			int j;
			printf("\n%s", ccStr);
			printf("\n");
			for (i = 0; i < rows; i++) {
				for (j = 0; j < CCB4_ELMNTS; j++) {
					printf("%d", ccPattern[i][j]);
				}
				printf("\n");
			}
		}
#endif

		if (ctx->bmp) {
			// note: BMP is bottom to top inverted
			gs1_bmpHeader(ctx->pixMult*EAN13_W, ctx->pixMult*(rows*2 + 6 + EAN13_H), ctx->outfp);

			// EAN-13
			gs1_printElmnts(ctx, &prints);

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// Composite Component
			prints.elmCnt = CCB4_ELMNTS;
			prints.height = ctx->pixMult*2;
			prints.leftPad = EAN13_L_PAD;
			prints.rightPad = EAN13_R_PAD;
			for (i = rows-1; i >= 0; i--) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}
		}
		else {
			gs1_tifHeader(ctx->pixMult*EAN13_W, ctx->pixMult*(rows*2 + 6 + EAN13_H), ctx->outfp);

			// Composite Component
			prints.elmCnt = CCB4_ELMNTS;
			prints.height = ctx->pixMult*2;
			prints.leftPad = EAN13_L_PAD;
			prints.rightPad = EAN13_R_PAD;
			for (i = 0; i < rows; i++) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// EAN-13
			prints.elmCnt = EAN13_ELMNTS;
			prints.pattern = linPattern;
			prints.height = ctx->pixMult*EAN13_H;
			prints.leftPad = 0;
			prints.rightPad = 0;
			gs1_printElmnts(ctx, &prints);
		}
	}
	else { // primary only
		if (ctx->bmp) {
			gs1_bmpHeader(ctx->pixMult*EAN13_W, ctx->pixMult*EAN13_H, ctx->outfp);
		}
		else {
			gs1_tifHeader(ctx->pixMult*EAN13_W, ctx->pixMult*EAN13_H, ctx->outfp);
		}

		// EAN-13
		gs1_printElmnts(ctx, &prints);
	}
	return;
}


#define EAN8_ELMNTS	45	// includes qz's
#define EAN8_W		81	// includes 7X quiet zones
#define EAN8_H		60	// total ht in x

#define EAN8_L_PAD	2	// EAN-8 7-X qz - CCA-2 offset
#define EAN8_R_PAD	5	// EAN-8 WIDTH - MAX_WIDTH of cca - EAN8_L_PAD
#define EAN8_L_PADB	8	// EAN-8 left pad for ccb

// call with str = 8-digit primary with check digit = 0
static bool EAN8enc(uint8_t str[], uint8_t pattern[] ) {

	static const uint16_t upcTblA[10] = {	0x3211, 0x2221, 0x2122, 0x1411, 0x1132,
					0x1231, 0x1114, 0x1312, 0x1213, 0x3112 };
	static const uint8_t lGuard[4] = { 7,1,1,1 };
	static const uint8_t center[5] = { 1,1,1,1,1 };
	static const uint8_t rGuard[4] = { 1,1,1,7 };

	int i, j, bars, sNdx, pNdx;

	// calculate UPC parity
	for (j = 0, i = 0; i < 12; i += 2) {
		j += str[i] - '0';
		j += (str[i+1] - '0') * 3;
	}
	j = (j%10);
	if (j > 0) {
		j = 10 - j;
	}
	str[12] = (uint8_t)(j + '0');

	sNdx = 5;
	pNdx = 0;
	for (i = 0; i < 4; i++) {
		pattern[pNdx++] = lGuard[i];
	}
	for (i = 0; i < 4; i++) {
		bars = upcTblA[str[sNdx++]-'0'];
		for (j = 12; j >= 0; j -= 4) {
			pattern[pNdx++] = (bars >> j) & 0xf;
		}
	}
	for (i = 0; i < 5; i++) {
		pattern[pNdx++] = center[i];
	}
	for (i = 0; i < 4; i++) {
		bars = upcTblA[str[sNdx++]-'0'];
		for (j = 12; j >= 0; j -= 4) {
			pattern[pNdx++] = (bars >> j) & 0xf;
		}
	}
	for (i = 0; i < 4; i++) {
		pattern[pNdx++] = rGuard[i];
	}
	return(true);
}

void gs1_EAN8(gs1_encoder *ctx) {

	struct sPrints prints;
	struct sPrints sepPrnt;

	uint8_t linPattern[EAN8_ELMNTS];
	uint8_t sepPat1[5] = { 7,1,EAN8_W-16,1,7 }; // separator pattern 1
	uint8_t sepPat2[5] = { 6,1,EAN8_W-14,1,6 }; // separator pattern 2

	uint8_t (*ccPattern)[CCB4_ELMNTS] = ctx->ccPattern;

	char primaryStr[14+1];
	char tempStr[28+1];

	int i;
	int rows, ccFlag;
	char *ccStr;
	int lpadCC;
	int lpadEAN;
	int elmntsCC;

	ccStr = strchr(ctx->dataStr, '|');
	if (ccStr == NULL) ccFlag = false;
	else {
		ccFlag = true;
		ccStr[0] = '\0'; // separate primary data
		ccStr++; // point to secondary data
	}

	if (strlen(ctx->dataStr) > 12) {
		sprintf(ctx->errMsg, "primary data exceeds 12 digits");
		ctx->errFlag = true;
		return;
	}

	strcpy(tempStr, "000000000000");
	strcat(tempStr, ctx->dataStr);
	strcat(tempStr, "0"); // check digit = 0 for now
	strcpy(primaryStr, tempStr + strlen(tempStr) - 13);

	if (!EAN8enc((uint8_t*)primaryStr, linPattern) || ctx->errFlag) return;
#if PRNT
	printf("\n%s", primaryStr);
	printf("\n");
	for (i = 0; i < EAN8_ELMNTS; i++) {
		printf("%d", linPattern[i]);
	}
	printf("\n");
#endif
	ctx->line1 = true; // so first line is not Y undercut
	// init most likely prints values
	lpadEAN = 0;
	lpadCC = EAN8_L_PAD;
	elmntsCC = CCA3_ELMNTS;
	prints.elmCnt = EAN8_ELMNTS;
	prints.pattern = linPattern;
	prints.guards = false;
	prints.height = ctx->pixMult*EAN8_H;
	prints.leftPad = 0;
	prints.rightPad = 0;
	prints.whtFirst = true;
	prints.reverse = false;
	// init most likely separator values
	sepPrnt.elmCnt = 5;
	sepPrnt.pattern = sepPat1;
	sepPrnt.guards = false;
	sepPrnt.height = ctx->pixMult*2;
	sepPrnt.leftPad = 0;
	sepPrnt.rightPad = 0;
	sepPrnt.whtFirst = true;
	sepPrnt.reverse = false;
	if (ccFlag) {
		if (!((rows = gs1_CC3enc(ctx, (uint8_t*)ccStr, ccPattern)) > 0) || ctx->errFlag) return;
		if (rows > MAX_CCA3_ROWS) { // CCB composite
			lpadEAN = EAN8_L_PADB;
			lpadCC = 0;
			elmntsCC = CCB3_ELMNTS;
			prints.leftPad = EAN8_L_PADB;
			sepPrnt.leftPad = EAN8_L_PADB;
		}
#if PRNT
		{
			int j;
			printf("\n%s", ccStr);
			printf("\n");
			for (i = 0; i < rows; i++) {
				for (j = 0; j < elmntsCC; j++) {
					printf("%d", ccPattern[i][j]);
				}
			}
			printf("\n");
		}
#endif

		if (ctx->bmp) {
			// note: BMP is bottom to top inverted
			gs1_bmpHeader(ctx->pixMult*(EAN8_W+lpadEAN), ctx->pixMult*(rows*2 + 6 + EAN8_H), ctx->outfp);

			// EAN-8
			gs1_printElmnts(ctx, &prints);

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// Composite Component
			prints.elmCnt = elmntsCC;
			prints.height = ctx->pixMult*2;
			prints.leftPad = lpadCC;
			prints.rightPad = EAN8_R_PAD;
			for (i = rows-1; i >= 0; i--) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}
		}
		else {
			gs1_tifHeader(ctx->pixMult*(EAN8_W+lpadEAN), ctx->pixMult*(rows*2 + 6 + EAN8_H), ctx->outfp);

			// Composite Component
			prints.elmCnt = elmntsCC;
			prints.height = ctx->pixMult*2;
			prints.leftPad = lpadCC;
			prints.rightPad = EAN8_R_PAD;
			for (i = 0; i < rows; i++) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// EAN-8
			prints.elmCnt = EAN8_ELMNTS;
			prints.pattern = linPattern;
			prints.height = ctx->pixMult*EAN8_H;
			prints.leftPad = lpadEAN;
			prints.rightPad = 0;
			gs1_printElmnts(ctx, &prints);
		}
	}
	else { // primary only
		if (ctx->bmp) {
			gs1_bmpHeader(ctx->pixMult*EAN8_W, ctx->pixMult*EAN8_H, ctx->outfp);
		}
		else {
			gs1_tifHeader(ctx->pixMult*EAN8_W, ctx->pixMult*EAN8_H, ctx->outfp);
		}

		// EAN-8
		gs1_printElmnts(ctx, &prints);
	}
	return;
}


#define UPCE_ELMNTS	35	// includes qz's
#define UPCE_W		65	// includes 7X quiet zones
#define UPCE_H		74	// total ht in x

#define UPCE_L_PAD	3	// UPC-E 7X qz - 4X
#define UPCE_R_PAD	5	// UPCE_W - MAX_WIDTH - UPCE_L_PAD

// call with str = 13-digit primary with check digit = 0
static bool UPCEenc(gs1_encoder *ctx, uint8_t str[], uint8_t pattern[] ) {

	static const uint16_t upcTblA[10] = {	0x3211, 0x2221, 0x2122, 0x1411, 0x1132,
					0x1231, 0x1114, 0x1312, 0x1213, 0x3112 };
	static const uint16_t upcTblB[10] = {	0x1123, 0x1222, 0x2212, 0x1141, 0x2311,
					0x1321, 0x4111, 0x2131, 0x3121, 0x2113 };
	static const uint16_t abArr[10] = { 0x07,0x0B,0x0D,0x0E,0x13,0x19,0x1C,0x15,0x16,0x1A };
	static const uint8_t lGuard[4] = { 7,1,1,1 };
	static const uint8_t rGuard[7] = { 1,1,1,1,1,1,7 };

	uint8_t data6[6+1];

	int i, j, abMask, bars, sNdx, pNdx, abBits;

	// calculate UPC parity
	for (j = 0, i = 0; i < 12; i += 2) {
		j += str[i] - '0';
		j += (str[i+1] - '0') * 3;
	}
	j = (j%10);
	if (j > 0) {
		j = 10 - j;
	}
	str[12] = (uint8_t)(j + '0');

	for (i = 0; i < 5; i++) {
		data6[i] = str[i+2];
	}
	if (str[4] >= '0' && str[4] <= '2' &&
			str[5] == '0' && str[6] == '0' && str[7] == '0' && str[8] == '0') {
		// 00abc0000hij = abhijc, where c = 0-2
		data6[2] = str[9];
		data6[3] = str[10];
		data6[4] = str[11];
		data6[5] = str[4];
	}
	else if (str[5] == '0' && str[6] == '0' && str[7] == '0' &&
			str[8] == '0' && str[9] == '0') {
		// 00abc00000ij = abcij3
		data6[3] = str[10];
		data6[4] = str[11];
		data6[5] = '3';
	}
	else if (str[6] == '0' && str[7] == '0' &&
			str[8] == '0' && str[9] == '0' && str[10] == '0') {
		// 00abcd00000j = abcdj4
		data6[4] = str[11];
		data6[5] = '4';
	}
	else if (str[11] >= '5' && str[11] <= '9' && str[7] == '0' &&
			str[8] == '0' && str[9] == '0' && str[10] == '0') {
		// 00abcde0000j = abcdej where j = 5-9
		data6[5] = str[11];
	}
	else {
		strcpy(ctx->errMsg, "Data cannot be converted to UPC-E");
		ctx->errFlag = true;
		return(false);
	}

	data6[6] = '\0';
#if PRNT
	printf("\n%s", data6);
#endif

	sNdx = 0;
	pNdx = 0;
	for (i = 0; i < 4; i++) {
		pattern[pNdx++] = lGuard[i];
	}
	abBits = abArr[str[12]-'0'];
	for (abMask = 0x20, i = 0; i < 6; abMask >>= 1, i++) {
		if ((abBits & abMask) != 0) {
			bars = upcTblA[data6[sNdx++]-'0'];
		}
		else {
			bars = upcTblB[data6[sNdx++]-'0'];
		}
		for (j = 12; j >= 0; j -= 4) {
			pattern[pNdx++] = (bars >> j) & 0xf;
		}
	}
	for (i = 0; i < 7; i++) {
		pattern[pNdx++] = rGuard[i];
	}
	return(true);
}

void gs1_UPCE(gs1_encoder *ctx) {

	struct sPrints prints;
	struct sPrints sepPrnt;

	uint8_t linPattern[UPCE_ELMNTS];
	uint8_t sepPat1[5] = { 7,1,UPCE_W-16,1,7 }; // separator pattern 1
	uint8_t sepPat2[5] = { 6,1,UPCE_W-14,1,6 }; // separator pattern 2

	uint8_t (*ccPattern)[CCB4_ELMNTS] = ctx->ccPattern;

	char primaryStr[14+1];
	char tempStr[28+1];

	int i;
	int rows, ccFlag;
	char *ccStr;

	ccStr = strchr(ctx->dataStr, '|');
	if (ccStr == NULL) ccFlag = false;
	else {
		ccFlag = true;
		ccStr[0] = '\0'; // separate primary data
		ccStr++; // point to secondary data
	}

	if (strlen(ctx->dataStr) > 12) {
		sprintf(ctx->errMsg, "primary data exceeds 12 digits");
		ctx->errFlag = true;
		return;
	}

	strcpy(tempStr, "000000000000");
	strcat(tempStr, ctx->dataStr);
	strcat(tempStr, "0"); // check digit = 0 for now
	strcpy(primaryStr, tempStr + strlen(tempStr) - 13);

	if (!UPCEenc(ctx, (uint8_t*)primaryStr, linPattern) || ctx->errFlag) return;
#if PRNT
	printf("\n%s", primaryStr);
	printf("\n");
	for (i = 0; i < UPCE_ELMNTS; i++) {
		printf("%d", linPattern[i]);
	}
	printf("\n");
#endif
	ctx->line1 = true; // so first line is not Y undercut
	// init most likely prints values
	prints.elmCnt = UPCE_ELMNTS;
	prints.pattern = linPattern;
	prints.guards = false;
	prints.height = ctx->pixMult*UPCE_H;
	prints.leftPad = 0;
	prints.rightPad = 0;
	prints.whtFirst = true;
	prints.reverse = false;
	// init most likely separator values
	sepPrnt.elmCnt = 5;
	sepPrnt.pattern = sepPat1;
	sepPrnt.guards = false;
	sepPrnt.height = ctx->pixMult*2;
	sepPrnt.leftPad = 0;
	sepPrnt.rightPad = 0;
	sepPrnt.whtFirst = true;
	sepPrnt.reverse = false;
	if (ccFlag) {
		if (!((rows = gs1_CC2enc(ctx, (uint8_t*)ccStr, ccPattern)) > 0) || ctx->errFlag) return;
#if PRNT
		{
			int j;
			printf("\n%s", ccStr);
			printf("\n");
			for (i = 0; i < rows; i++) {
				for (j = 0; j < CCB2_ELMNTS; j++) {
					printf("%d", ccPattern[i][j]);
				}
				printf("\n");
			}
		}
#endif

		if (ctx->bmp) {
			// note: BMP is bottom to top inverted
			gs1_bmpHeader(ctx->pixMult*UPCE_W, ctx->pixMult*(rows*2 + 6 + UPCE_H), ctx->outfp);

			// UPC-E
			gs1_printElmnts(ctx, &prints);

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// Composite Component
			prints.elmCnt = CCB2_ELMNTS;
			prints.height = ctx->pixMult*2;
			prints.leftPad = UPCE_L_PAD;
			prints.rightPad = UPCE_R_PAD;
			for (i = rows-1; i >= 0; i--) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}
		}
		else {
			gs1_tifHeader(ctx->pixMult*UPCE_W, ctx->pixMult*(rows*2 + 6 + UPCE_H), ctx->outfp);

			// Composite Component
			prints.elmCnt = CCB2_ELMNTS;
			prints.height = ctx->pixMult*2;
			prints.leftPad = UPCE_L_PAD;
			prints.rightPad = UPCE_R_PAD;
			for (i = 0; i < rows; i++) {
				prints.pattern = ccPattern[i];
				gs1_printElmnts(ctx, &prints);
			}

			// CC separator
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat2;
			gs1_printElmnts(ctx, &sepPrnt);
			sepPrnt.pattern = sepPat1;
			gs1_printElmnts(ctx, &sepPrnt);

			// UPC-E
			prints.elmCnt = UPCE_ELMNTS;
			prints.pattern = linPattern;
			prints.height = ctx->pixMult*UPCE_H;
			prints.leftPad = 0;
			prints.rightPad = 0;
			gs1_printElmnts(ctx, &prints);
		}
	}
	else { // primary only
		if (ctx->bmp) {
			gs1_bmpHeader(ctx->pixMult*UPCE_W, ctx->pixMult*UPCE_H, ctx->outfp);
		}
		else {
			gs1_tifHeader(ctx->pixMult*UPCE_W, ctx->pixMult*UPCE_H, ctx->outfp);
		}

		// UPC-E
		gs1_printElmnts(ctx, &prints);
	}
	return;
}
