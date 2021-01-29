// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>

#include "kernel.h"
#include "db.h"

using namespace std;

extern int nStakeMaxAge;
extern int nStakeTargetSpacing;

// Modifier interval: time to elapse before new modifier is computed
// Set to 3-hour for production network and 20-minute for test network

unsigned int nModifierInterval = MODIFIER_INTERVAL;

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
    boost::ass