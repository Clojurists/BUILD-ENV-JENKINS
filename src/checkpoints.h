// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_CHECKPOINT_H
#define  BITCOIN_CHECKPOINT_H

#include <map>
#include "net.h"
#include "util.h"

#define CHECKPOINT_MAX_SPAN (60 * 60 * 4) // max 4 hours before latest block

#ifdef WIN32
#undef STRICT
#undef PERMISSIVE
#undef ADVISORY
#endif

class uint256;
class CBlockIndex;
class CSyncCh