#include "IrTransmitter.h"
#include <ir_NEC.h>

bool IrTransmitter::enqueue(const IrJob& job) {
  if (count_ == IR_QUEUE_SIZE) return false;
  queue_[tail_] = job;
  tail_ = (tail_ + 1) % IR_QUEUE_SIZE;
  ++count_;
  return true;
}

void IrTransmitter::loop() {
  if (!count_ || sending_) return;
  sending_ = true;
  IrJob& job = queue_[head_];
  Serial.printf("IR sending %s\n", job.name);
  switch (job.protocol) {
    case IrProtocol::Nec:
      if (job.frequency == 38000) sender_.sendNEC(job.code, job.bits);
      else sender_.sendGeneric(kNecHdrMark, kNecHdrSpace, kNecBitMark, kNecOneSpace,
                               kNecBitMark, kNecZeroSpace, kNecBitMark, kNecMinGap,
                               kNecMinCommandLength, job.code, job.bits, job.frequency,
                               true, 0, 33);
      break;
    case IrProtocol::Samsung:
      sender_.sendSAMSUNG(job.code, job.bits);
      break;
    case IrProtocol::Sony:
      if (job.frequency == 38000) sender_.sendSony38(job.code, job.bits);
      else if (job.frequency == 40000) sender_.sendSony(job.code, job.bits);
      else sender_.sendGeneric(2400, 600, 1200, 600, 600, 600, 0, 10000,
                               45000, job.code, job.bits, job.frequency, true, 2, 33);
      break;
    case IrProtocol::Rc5:
      if (!job.held) rc5Toggle_ = !rc5Toggle_;
      sender_.sendRC5((job.code & ~0x800ULL) | (rc5Toggle_ ? 0x800ULL : 0), job.bits);
      break;
    case IrProtocol::Rc6:
      if (!job.held) rc6Toggle_ = !rc6Toggle_;
      {
        uint64_t mask = job.bits == 36 ? 0x8000ULL : 0x10000ULL;
        sender_.sendRC6((job.code & ~mask) | (rc6Toggle_ ? mask : 0), job.bits);
      }
      break;
    case IrProtocol::Raw: sender_.sendRawWithDuty(job.timings, job.length, job.frequency, job.dutyCycle); break;
  }
  head_ = (head_ + 1) % IR_QUEUE_SIZE;
  --count_;
  sending_ = false;
}
