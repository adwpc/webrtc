/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/nack_module.h"

#include <algorithm>
#include <limits>

#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
const int kMaxPacketAge = 10000;
const int kMaxNackPackets = 1000;
const int kDefaultRttMs = 100;
const int kMaxNackRetries = 10;
const int kProcessFrequency = 50;
const int kProcessIntervalMs = 1000 / kProcessFrequency;
const int kMaxReorderedPackets = 128;
const int kNumReorderingBuckets = 10;
const int kDefaultSendNackDelayMs = 0;

int64_t GetSendNackDelay() {
  int64_t delay_ms = strtol(
      webrtc::field_trial::FindFullName("WebRTC-SendNackDelayMs").c_str(),
      nullptr, 10);
  if (delay_ms > 0 && delay_ms <= 20) {
    RTC_LOG(LS_INFO) << "SendNackDelay is set to " << delay_ms;
    return delay_ms;
  }
  return kDefaultSendNackDelayMs;
}
}  // namespace

NackModule::NackInfo::NackInfo()
    : seq_num(0), send_at_seq_num(0), sent_at_time(-1), retries(0) {}

NackModule::NackInfo::NackInfo(uint16_t seq_num,
                               uint16_t send_at_seq_num,
                               int64_t created_at_time)
    : seq_num(seq_num),
      send_at_seq_num(send_at_seq_num),
      created_at_time(created_at_time),
      sent_at_time(-1),
      retries(0) {}

NackModule::NackModule(Clock* clock,
                       NackSender* nack_sender,
                       KeyFrameRequestSender* keyframe_request_sender)
    : clock_(clock),
      nack_sender_(nack_sender),
      keyframe_request_sender_(keyframe_request_sender),
      reordering_histogram_(kNumReorderingBuckets, kMaxReorderedPackets),
      initialized_(false),
      rtt_ms_(kDefaultRttMs),
      newest_seq_num_(0),
      next_process_time_ms_(-1),
      send_nack_delay_ms_(GetSendNackDelay()) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(nack_sender_);
  RTC_DCHECK(keyframe_request_sender_);
}

int NackModule::OnReceivedPacket(uint16_t seq_num, bool is_keyframe) {
  return OnReceivedPacket(seq_num, is_keyframe, false);
}

int NackModule::OnReceivedPacket(uint16_t seq_num,
                                 bool is_keyframe,
                                 bool is_recovered) {
  rtc::CritScope lock(&crit_);
  // TODO(philipel): When the packet includes information whether it is
  //                 retransmitted or not, use that value instead. For
  //                 now set it to true, which will cause the reordering
  //                 statistics to never be updated.
  bool is_retransmitted = true;

  if (!initialized_) {//如果未初始化，初始化newest_seq_num_
    newest_seq_num_ = seq_num;
    if (is_keyframe)
      keyframe_list_.insert(seq_num);//如果是关键帧，sn插入keyframe list
    initialized_ = true;
    return 0;
  }

  // Since the |newest_seq_num_| is a packet we have actually received we know
  // that packet has never been Nacked.因为newest_seq_num_从未nack过，返回
  if (seq_num == newest_seq_num_)
    return 0;
  
  if (AheadOf(newest_seq_num_, seq_num)) {
    // An out of order packet has been received.乱序的包到来
    auto nack_list_it = nack_list_.find(seq_num);
    int nacks_sent_for_packet = 0;
    if (nack_list_it != nack_list_.end()) {//如果这个包之前已经有记录
      nacks_sent_for_packet = nack_list_it->second.retries;
      nack_list_.erase(nack_list_it);//把重新收到的包从nack_list_中移除掉
    }
    if (!is_retransmitted)
      UpdateReorderingStatistics(seq_num);
    return nacks_sent_for_packet;//返回这个包的nack发送次数
  }

  // Keep track of new keyframes.如果是关键帧插入关键帧列表
  if (is_keyframe)
    keyframe_list_.insert(seq_num);

  // And remove old ones so we don't accumulate keyframes.从关键帧列表删掉旧的sn
  auto it = keyframe_list_.lower_bound(seq_num - kMaxPacketAge);
  if (it != keyframe_list_.begin())
    keyframe_list_.erase(keyframe_list_.begin(), it);

  if (is_recovered) {//是否是rtx包
    recovered_list_.insert(seq_num);

    // Remove old ones so we don't accumulate recovered packets.
    auto it = recovered_list_.lower_bound(seq_num - kMaxPacketAge);
    if (it != recovered_list_.begin())
      recovered_list_.erase(recovered_list_.begin(), it);

    // Do not send nack for packets recovered by FEC or RTX.
    return 0;
  }
  //把从上次插入的sn到当前sn这个区间，插入nack_list_
  AddPacketsToNack(newest_seq_num_ + 1, seq_num);
  newest_seq_num_ = seq_num;
 //从nack_list 中取出需要发送 NACK 的序号列表, 如果某个 seq 请求次数超过 kMaxNackRetries = 10次则会从nack_list 中删除.
  // Are there any nacks that are waiting for this seq_num.
  std::vector<uint16_t> nack_batch = GetNackBatch(kSeqNumOnly);
  if (!nack_batch.empty()) {
    // This batch of NACKs is triggered externally; the initiator can
    // batch them with other feedback messages.
    nack_sender_->SendNack(nack_batch, /*buffering_allowed=*/true);
  }

  return 0;
}

void NackModule::ClearUpTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  nack_list_.erase(nack_list_.begin(), nack_list_.lower_bound(seq_num));
  keyframe_list_.erase(keyframe_list_.begin(),
                       keyframe_list_.lower_bound(seq_num));
  recovered_list_.erase(recovered_list_.begin(),
                        recovered_list_.lower_bound(seq_num));
}

void NackModule::UpdateRtt(int64_t rtt_ms) {
  rtc::CritScope lock(&crit_);
  rtt_ms_ = rtt_ms;
}

void NackModule::Clear() {
  rtc::CritScope lock(&crit_);
  nack_list_.clear();
  keyframe_list_.clear();
  recovered_list_.clear();
}

int64_t NackModule::TimeUntilNextProcess() {
  return std::max<int64_t>(next_process_time_ms_ - clock_->TimeInMilliseconds(),
                           0);
}

void NackModule::Process() {
  if (nack_sender_) {
    std::vector<uint16_t> nack_batch;
    {
      rtc::CritScope lock(&crit_);
      nack_batch = GetNackBatch(kTimeOnly);
    }

    if (!nack_batch.empty()) {
      // This batch of NACKs is triggered externally; there is no external
      // initiator who can batch them with other feedback messages.
      nack_sender_->SendNack(nack_batch, /*buffering_allowed=*/false);
    }
  }

  // Update the next_process_time_ms_ in intervals to achieve
  // the targeted frequency over time. Also add multiple intervals
  // in case of a skip in time as to not make uneccessary
  // calls to Process in order to catch up.
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (next_process_time_ms_ == -1) {
    next_process_time_ms_ = now_ms + kProcessIntervalMs;
  } else {
    next_process_time_ms_ = next_process_time_ms_ + kProcessIntervalMs +
                            (now_ms - next_process_time_ms_) /
                                kProcessIntervalMs * kProcessIntervalMs;
  }
}

bool NackModule::RemovePacketsUntilKeyFrame() {
  while (!keyframe_list_.empty()) {
    auto it = nack_list_.lower_bound(*keyframe_list_.begin());

    if (it != nack_list_.begin()) {
      // We have found a keyframe that actually is newer than at least one
      // packet in the nack list.
      nack_list_.erase(nack_list_.begin(), it);
      return true;
    }

    // If this keyframe is so old it does not remove any packets from the list,
    // remove it from the list of keyframes and try the next keyframe.
    keyframe_list_.erase(keyframe_list_.begin());
  }
  return false;
}

void NackModule::AddPacketsToNack(uint16_t seq_num_start,
                                  uint16_t seq_num_end) {
  // Remove old packets.
  auto it = nack_list_.lower_bound(seq_num_end - kMaxPacketAge);
  nack_list_.erase(nack_list_.begin(), it);

  // If the nack list is too large, remove packets from the nack list until
  // the latest first packet of a keyframe. If the list is still too large,
  // clear it and request a keyframe.
  uint16_t num_new_nacks = ForwardDiff(seq_num_start, seq_num_end);
  if (nack_list_.size() + num_new_nacks > kMaxNackPackets) {
    while (RemovePacketsUntilKeyFrame() &&
           nack_list_.size() + num_new_nacks > kMaxNackPackets) {
    }

    if (nack_list_.size() + num_new_nacks > kMaxNackPackets) {
      nack_list_.clear();
      RTC_LOG(LS_WARNING) << "NACK list full, clearing NACK"
                             " list and requesting keyframe.";
      keyframe_request_sender_->RequestKeyFrame();
      return;
    }
  }

  for (uint16_t seq_num = seq_num_start; seq_num != seq_num_end; ++seq_num) {
    // Do not send nack for packets that are already recovered by FEC or RTX
    if (recovered_list_.find(seq_num) != recovered_list_.end())
      continue;
    NackInfo nack_info(seq_num, seq_num + WaitNumberOfPackets(0.5),
                       clock_->TimeInMilliseconds());
    RTC_DCHECK(nack_list_.find(seq_num) == nack_list_.end());
    nack_list_[seq_num] = nack_info;
  }
}

std::vector<uint16_t> NackModule::GetNackBatch(NackFilterOptions options) {
  bool consider_seq_num = options != kTimeOnly;
  bool consider_timestamp = options != kSeqNumOnly;
  int64_t now_ms = clock_->TimeInMilliseconds();
  std::vector<uint16_t> nack_batch;
  auto it = nack_list_.begin();
  while (it != nack_list_.end()) {
    bool delay_timed_out =
        now_ms - it->second.created_at_time >= send_nack_delay_ms_;
    bool nack_on_rtt_passed = now_ms - it->second.sent_at_time >= rtt_ms_;
    bool nack_on_seq_num_passed =
        it->second.sent_at_time == -1 &&
        AheadOrAt(newest_seq_num_, it->second.send_at_seq_num);
    if (delay_timed_out && ((consider_seq_num && nack_on_seq_num_passed) ||
                            (consider_timestamp && nack_on_rtt_passed))) {//
      nack_batch.emplace_back(it->second.seq_num);
      ++it->second.retries;
      it->second.sent_at_time = now_ms;
      if (it->second.retries >= kMaxNackRetries) {
        RTC_LOG(LS_WARNING) << "Sequence number " << it->second.seq_num
                            << " removed from NACK list due to max retries.";
        it = nack_list_.erase(it);
      } else {
        ++it;
      }
      continue;
    }
    ++it;
  }
  return nack_batch;
}

void NackModule::UpdateReorderingStatistics(uint16_t seq_num) {
  RTC_DCHECK(AheadOf(newest_seq_num_, seq_num));
  uint16_t diff = ReverseDiff(newest_seq_num_, seq_num);
  reordering_histogram_.Add(diff);
}

int NackModule::WaitNumberOfPackets(float probability) const {
  if (reordering_histogram_.NumValues() == 0)
    return 0;
  return reordering_histogram_.InverseCdf(probability);
}

}  // namespace webrtc
