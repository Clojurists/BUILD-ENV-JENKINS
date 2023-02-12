#include <boost/assert.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>

#include <openssl/ec.h>
#include <openssl/err.h>

#include "keystore.h"
#include "main.h"
#include "script.h"
#include "wallet.h"

using namespace std;
using namespace boost::assign;

typedef vector<unsigned char> valtype;

extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);
extern bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CTransaction& txTo, unsigned int nIn,
                         bool fValidatePayToScriptHash, int nHashType);

BOOST_AUTO_TEST_SUITE(multisig_tests)

CScript
sign_multisig(CScript scriptPubKey, vector<CKey> keys, CTransaction transaction, int whichIn)
{
    uint256 hash = SignatureHash(scriptPubKey, transaction, whichIn, SIGHASH_ALL);

    CScript result;
    result << OP_0; // CHECKMULTISIG bug workaround
    BOOST_FOREACH(CKey key, keys)
    {
        vector<unsigned char> vchSig;
        BOOST_CHECK(key.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        result << vchSig;
    }
    return result;
}

BOOST_AUTO_TEST_CASE(multisig_verify)
{
    CKey key[4];
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    CScript a_and_b;
    a_and_b << OP_2 << key[0].GetPubKey() << key[1].GetPubKey() << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << key[0].GetPubKey() << key[1].GetPubKey() << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << key[0].GetPubKey() << key[1].GetPubKey() << key[2].GetPubKey() << OP_3 << OP_CHECKMULTISIG;

    CTransaction txFrom;  // Funding transaction
    txFrom.vout.resize(3);
    txFr