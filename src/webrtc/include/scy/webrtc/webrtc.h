﻿///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @defgroup webrtc WebRTC module
///
/// The `webrtc` module contains WebRTC integrations.
///
/// @addtogroup webrtc
/// @{


#ifndef SCY_WebRTC_WEBRTC_H
#define SCY_WebRTC_WEBRTC_H

#define USE_BUILTIN_SW_CODECS 1


namespace scy {
namespace wrtc {
// namespace webrtc {


/// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

/// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

/// Labels for ICE streams.
const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";

} } // namespace scy::wrtc


#endif // SCY_WebRTC_WEBRTC_H


/// @\}
