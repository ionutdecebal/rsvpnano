// One connection for the start command and result stream, including USB chunks
// that split a protocol marker. No host clock values become device measurements.
export async function collectReader(port, {onText, onStarted = () => {}, signal,
  timeoutMs = 300000, retryMs = 1000}) {
  let reader, writer, timer, retry, writing = false, failure = null, started = false, recent = '';
  const fail = error => {
    failure ||= error;
    reader?.cancel().catch(() => {});
    writer?.abort(error).catch(() => {});
  };
  const abort = () => fail(new Error('Collection stopped. This run is incomplete; the device may still be testing.'));
  try {
    await port.open({baudRate:115200, bufferSize:65536});
    await port.setSignals({dataTerminalReady:false, requestToSend:false});
    reader = port.readable.getReader();
    writer = port.writable.getWriter();
    signal?.addEventListener('abort', abort, {once:true});
    if (signal?.aborted) abort();
    timer = setTimeout(() => fail(new Error('Timed out waiting for the complete device result. Reset the Nano, prepare the drive again, eject and reconnect.')), timeoutMs);
    const request = async () => {
      if (started || writing || failure) return;
      writing = true;
      try { await writer.write(new TextEncoder().encode('reader-start\n')); }
      catch (error) { fail(error); }
      finally { writing = false; }
    };
    retry = setInterval(request, retryMs);
    void request();
    const decoder = new TextDecoder();
    while (!failure) {
      const {value, done} = await reader.read();
      if (done) break;
      const text = decoder.decode(value, {stream:true});
      onText(text);
      recent = (recent + text).slice(-4096);
      if (!started && /\[bench\] start /.test(recent)) { started = true; clearInterval(retry); onStarted(); }
      if (/\[bench\] done(?:\r?\n)/.test(recent.replace(/\x1b\[[0-9;]*m/g, ''))) return;
    }
    throw failure || new Error('USB disconnected before the completion marker. The result is incomplete.');
  } finally {
    clearInterval(retry); clearTimeout(timer);
    signal?.removeEventListener('abort', abort);
    await reader?.cancel().catch(() => {});
    reader?.releaseLock();
    if (writing) await writer?.abort().catch(() => {});
    writer?.releaseLock();
    await port.close().catch(() => {});
  }
}
