/* Hero platform contract v1: hosts execute requested settings exactly or fail closed. */
(function(root){'use strict';
 const fail=m=>{throw new Error('Hero render contract: '+m);};
 const num=(v)=>Number(v);
 function assertRequest(s){
  if(!s||typeof s!=='object')fail('render settings missing');
  const w=num(s.width),h=num(s.height),rate=num(s.audio_sample_rate),bits=num(s.audio_bit_depth),fn=num(s.fps_num),fd=num(s.fps_den),vb=num(s.video_bitrate),ab=num(s.audio_bitrate),gain=num(s.master_gain),dur=num(s.duration);
  if(!Number.isInteger(w)||!Number.isInteger(h)||w<16||h<16||w>7680||h>4320||w%2||h%2)fail('requested dimensions unsupported');
  if(![44100,48000,88200,96000,176400,192000].includes(rate))fail('requested sample rate unsupported');
  if(![16,24,32].includes(bits))fail('requested PCM bit depth unsupported');
  if(!Number.isInteger(fn)||!Number.isInteger(fd)||fn<1||fn>240||fd<1)fail('requested frame rate invalid');
  if(!Number.isFinite(vb)||vb<100000||vb>500000000||!Number.isFinite(ab)||ab<16000||ab>1536000)fail('requested bitrate outside engine policy');
  if(!Number.isFinite(gain)||gain<0||gain>16||!Number.isFinite(dur)||dur<=0||dur>86400)fail('gain or duration invalid');
  if(!['mp4','mov','mkv','webm','avi','wav','flac'].includes(s.container))fail('container unsupported');
  if(s.video&&['wav','flac'].includes(s.container))fail('audio-only container cannot contain video');
  if(!s.video&&!['wav','flac'].includes(s.container))fail('audio-only output requires WAV or FLAC');
  return s;
 }
 function same(a,b){if(typeof a==='number'||typeof b==='number')return Number.isFinite(Number(a))&&Number.isFinite(Number(b))&&Math.abs(Number(a)-Number(b))<1e-9;return a===b;}
 function assertVerified(result,expected){
  assertRequest(expected);
  if(!result||!['ok','completed'].includes(result.status))fail('host did not confirm a completed native render');
  if(!(result.outputUri||result.outputPath)||!(Number(result.bytes)>0))fail('output file is missing or empty');
  const echoed=result.requestedSettings;
  if(!echoed||typeof echoed!=='object')fail('host did not echo the exact requested settings');
  for(const k of ['container','video','video_codec','audio_codec','width','height','fps_num','fps_den','video_bitrate','audio_bitrate','audio_sample_rate','audio_bit_depth','master_gain','duration'])if(!same(echoed[k],expected[k]))fail('host changed or omitted requested setting: '+k);
  const streams=result.verification&&result.verification.streams;
  if(!Array.isArray(streams))fail('output metadata was not independently verified');
  const vs=streams.find(x=>x.codec_type==='video'),as=streams.find(x=>x.codec_type==='audio');
  if(!as)fail('verified output has no audio stream');
  const audioCodec=expected.audio_codec==='pcm'?({16:'pcm_s16le',24:'pcm_s24le',32:'pcm_s32le'}[expected.audio_bit_depth]):expected.audio_codec;
  if(as.codec_name!==audioCodec)fail('verified audio codec differs from requested codec');
  if(Number(as.sample_rate)!==Number(expected.audio_sample_rate))fail('verified audio sample rate differs from requested rate');
  if(expected.audio_codec==='pcm'&&Number(as.bits_per_sample||as.bits_per_raw_sample)!==Number(expected.audio_bit_depth))fail('verified PCM bit depth differs from requested bit depth');
  if(expected.video){
   if(!vs)fail('verified output has no video stream');
   const codec=({h264:'h264',hevc:'hevc',av1:'av1',vp9:'vp9',prores:'prores',ffv1:'ffv1'})[expected.video_codec];
   if(vs.codec_name!==codec)fail('verified video codec differs from requested codec');
   if(Number(vs.width)!==Number(expected.width)||Number(vs.height)!==Number(expected.height))fail('verified dimensions differ from requested dimensions');
   const [n,d]=(vs.avg_frame_rate||'0/1').split('/').map(Number);
   if(!d||Math.abs(n*Number(expected.fps_den)-Number(expected.fps_num)*d)>Math.max(1,Number(expected.fps_den)))fail('verified frame rate differs from requested frame rate');
  } else if(vs) fail('unexpected video stream in audio-only output');
  return result;
 }
 root.HeroPlatformContract=Object.freeze({assertRequest,assertVerified});
})(typeof window!=='undefined'?window:globalThis);
