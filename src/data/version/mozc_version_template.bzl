MAJOR=2
MINOR=18
BUILD=2568
REVISION=102
# CAUTION: NACL_DICTIONARY_VERSION is going to be migrated to ENGINE_VERSION.
# NACL_DICTIONARY_VERSION is the target version of the system dictionary to be
# downloaded by NaCl Mozc.
# TODO(noriyukit): Migrate this version to ENGINE_VERSION.
NACL_DICTIONARY_VERSION=22
# This version represents the version of Mozc IME engine (converter, predictor,
# etc.).  This version info is included both in the Mozc server and in the Mozc
# data set file so that the Mozc server can accept only the compatible version
# of data set file.  The engine version must be incremented when:
#  * POS matcher definition and/or conversion models were changed,
#  * New data are added to the data set file, and/or
#  * Any changes that loose data compatibility are made.
ENGINE_VERSION=22
# This version is used to manage the data version and is included only in the
# data set file.  DATA_VERSION can be incremented without updating
# ENGINE_VERSION as long as it's compatible with the engine.
DATA_VERSION=0
