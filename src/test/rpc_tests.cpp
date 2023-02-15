#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "base58.h"
#include "util.h"
#include "bitcoinrpc.h"

using namespace std;
using namespace json_spirit;

BOOST_AUTO_TEST_SUITE(rpc_tests)

static Array
create