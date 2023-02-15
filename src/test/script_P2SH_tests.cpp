
#include <boost/assert.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "../main.h"
#include "../script.h"
#include "../wallet.h"

using namespace std;

// Test routines internal to script.cpp:
extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);
extern bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                         bool fValidatePayToScriptHash, int nHashType);

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s);
    return sSerialized;
}

static bool
Verify(const CScript& scriptSig, const CScript& scriptPubKey, bool fStrict)
{
    // Create dummy to/from transactions:
    CTransaction txFrom;
    txFrom.vout.resize(1);
    txFrom.vout[0].scriptPubKey = scriptPubKey;

    CTransaction txTo;
    txTo.vin.resize(1);
    txTo.vout.resize(1);
    txTo.vin[0].prevout.n = 0;
    txTo.vin[0].prevout.hash = txFrom.GetHash();
    txTo.vin[0].scriptSig = scriptSig;
    txTo.vout[0].nValue = 1;

    return VerifyScript(scriptSig, scriptPubKey, txTo, 0, fStrict, 0);
}