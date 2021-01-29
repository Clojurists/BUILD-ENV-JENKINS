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
    boost::assign::map_list_of
    (     0, 0x0e00670bu )
    (  1001, 0xfc258f54u )
    ( 10011, 0x6f01954bu )
    (100001, 0x7a965716u )
    (150001, 0xf901006du )
    (184001, 0x86191c79u )
    (200001, 0x8cbb46fcu )
    (300001, 0x5d2caf58u )
    (400001, 0xacd6bdfeu )
    (500001, 0x656fb6e3u )
    (600001, 0x8b00b6f2u )
	;

// Get time weight
int64 GetWeight(int64 nIntervalBeginning, int64 nIntervalEnd)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low

    return min(nIntervalEnd - nIntervalBeginning - nStakeMinAge, (int64)nStakeMaxAge);
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64& nStakeModifier, int64& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
 