/*
 * Sigh.  We can link with xtcdata, or we can include these directly.
 *
 * There is an issue with Sequence and Timestamp, so one is here and one is in xtcrdr.cc.
 */
#include"pdsdata/xtc/src/XtcFileIterator.cc"
#include"pdsdata/xtc/src/DetInfo.cc"
#include"pdsdata/xtc/src/TypeId.cc"
//#include"pdsdata/xtc/src/Sequence.cc"
#include"pdsdata/xtc/src/TimeStamp.cc"
#include"pdsdata/xtc/src/ClockTime.cc"
#include"pdsdata/xtc/src/Src.cc"
