#include "IrTransmitter.h"

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
    case IrProtocol::Nec: sender_.sendNEC(job.code, job.bits); break;
    case IrProtocol::Sony: sender_.sendSony(job.code, job.bits); break;
    case IrProtocol::Rc5: sender_.sendRC5(job.code, job.bits); break;
    case IrProtocol::Rc6: sender_.sendRC6(job.code, job.bits); break;
    case IrProtocol::Raw: sender_.sendRaw(job.timings, job.length, job.frequency / 1000); break;
  }
  head_ = (head_ + 1) % IR_QUEUE_SIZE;
  --count_;
  sending_ = false;
}
