// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/version.hpp>
#if defined(WIN32) && BOOST_VERSION == 104900
#define BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME
#define BOOST_INTERPROCESS_HAS_KERNEL_BOOTTIME
#endif

#include "qtipcserver.h"
#include "guiconstants.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/version.hpp>

#if defined(WIN32) && (!defined(BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTIME) || !defined(BOOST_INTERPROCESS_HAS_KERNEL_BOOTTIME) || BOOST_VERSION < 104900)
#warning Compiling without BOOST_INTERPROCESS_HAS_WINDOWS_KERNEL_BOOTTI