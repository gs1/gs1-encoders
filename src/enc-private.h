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

#ifndef ENC_PRIVATE_H
#define ENC_PRIVATE_H

#include <stdint.h>
#include "enc.h"
#include "rss14.h"
#include "rsslim.h"
#include "rssexp.h"
#include "ean.h"
#include "ucc128.h"

#define PRNT 0 // prints symbol data if 1

struct sPrints {
	int elmCnt;
	int leftPad;
	int rightPad;
	int guards;
	int height;
	int whtFirst;
	int reverse;
	uint8_t *pattern;
};

// globals
int errFlag;
char* errMsg;
int rowWidth;
int line1;
int linFlag; // tells pack whether linear or cc is being encoded

#endif /* ENC_PRIVATE_H */
