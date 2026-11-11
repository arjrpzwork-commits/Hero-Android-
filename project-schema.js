/* Hero Vid Studios project schema v2. Recovered/rebuilt source; no dependencies. */
(function(root){'use strict';
  const finite=n=>typeof n==='number'&&Number.isFinite(n);
  const fail=m=>{throw new Error('Hero project validation: '+m);};
  function assertProject(p){
    if(!p||typeof p!=='object'||Array.isArray(p))fail('project must be an object');
    if(!(['HeroProject','HeroBrowserSource'].includes(p.format))||p.version!==2)fail('unsupported project format/version');
    if(!Array.isArray(p.sources)||!Array.isArray(p.clips))fail('sources and clips must be arrays');
    if(p.format==='HeroProject'&&p.mediaEmbedded!==false)fail('media must remain external; source files are never silently embedded');
    const sourceIds=new Set();
    for(const s of p.sources){
      if(!s||typeof s.id!=='string'||!s.id.trim()||sourceIds.has(s.id))fail('source IDs must be non-empty and unique');
      sourceIds.add(s.id);
      if(typeof s.name!=='string'||!s.name.trim())fail('source name is required');
      if(!['audio','video'].includes(s.kind))fail('source kind must be audio or video');
      if(!finite(s.size)||s.size<0||!finite(s.duration)||s.duration<=0)fail('source size/duration is invalid');
    }
    const clipIds=new Set();
    for(const c of p.clips){
      if(!c||typeof c.id!=='string'||!c.id.trim()||clipIds.has(c.id))fail('clip IDs must be non-empty and unique');
      clipIds.add(c.id);
      if(!sourceIds.has(c.source))fail('clip refers to an unresolved source');
      const src=p.sources.find(s=>s.id===c.source);
      if(c.kind!==src.kind)fail('clip kind does not match its source');
      if(![c.start,c.in,c.out,c.gain].every(finite)||c.start<0||c.in<0||c.out<=c.in||c.out>src.duration+1e-6||c.gain<0)fail('clip timing or gain is invalid');
      if(c.track!==undefined&&(!Number.isInteger(c.track)||c.track<0))fail('clip track index is invalid');
      if(c.automation!==undefined){
        if(!Array.isArray(c.automation))fail('automation must be an array');
        let prev=-Infinity; for(const k of c.automation){if(!k||!finite(k.time)||!finite(k.gain)||k.time<0||k.time>c.out-c.in+1e-6||k.time<prev||k.gain<0)fail('automation points are invalid or unordered');prev=k.time;}
      }
    }
    const tc=p.trackCounts||{};const vc=tc.video??1,ac=tc.audio??1;
    if(!Number.isInteger(vc)||!Number.isInteger(ac)||vc<1||ac<1)fail('video and audio track counts must be positive integers');
    if(p.settings!==undefined&&(!p.settings||typeof p.settings!=='object'||Array.isArray(p.settings)))fail('settings must be an object');
    if(p.timeline!==undefined&&(!p.timeline||typeof p.timeline!=='object'||Array.isArray(p.timeline)))fail('timeline settings must be an object');
    if(p.timeline?.snapToGrid!==undefined&&typeof p.timeline.snapToGrid!=='boolean')fail('timeline snapToGrid must be boolean');
    if(p.audioTrackGains!==undefined&&(!Array.isArray(p.audioTrackGains)||p.audioTrackGains.some(x=>!finite(x)||x<0)))fail('audio track gains are invalid');
    if(p.audioTrackMutes!==undefined&&(!Array.isArray(p.audioTrackMutes)||p.audioTrackMutes.some(x=>typeof x!=='boolean')))fail('audio track mute values are invalid');
    if(p.audioTrackSolos!==undefined&&(!Array.isArray(p.audioTrackSolos)||p.audioTrackSolos.some(x=>typeof x!=='boolean')))fail('audio track solo values are invalid');
    return p;
  }
  root.HeroProjectSchema=Object.freeze({assert:assertProject,validate(p){try{assertProject(p);return {valid:true,errors:[]};}catch(e){return {valid:false,errors:[e.message]};}}});
})(typeof window!=='undefined'?window:globalThis);
