// SPDX-License-Identifier: GPL-3.0-only
import {reportProgress} from './update-progress.js?v=20261013';
// Only the same-origin loopback preview can own the local USB transport.
// Hosted sites retain their browser transport; no local-machine probing/CORS.
let nativeAvailable=false;
export async function hasNativeSmk(model,fetcher=fetch){
 if(model!=='smk-pro'||!['127.0.0.1','localhost'].includes(location.hostname))return false;
 if(nativeAvailable)return true;
 try{
  const response=await fetcher('/api/smk/capabilities',{cache:'no-store',signal:AbortSignal.timeout(2000)});
  if(!response.ok)return false;
  const result=await response.json();nativeAvailable=result.available===true&&result.models?.includes(model);return nativeAvailable;
 }catch{throw Error('The local SMK USB service could not be reached. Keep the device powered and restart the local preview before retrying.');}
}
export async function runNativeSmk(action,log=()=>{},fetcher=fetch){
 async function request(url,options={}){
  try{return await fetcher(url,{...options,signal:AbortSignal.timeout(10000)});}
  catch{throw Error('The local USB connection was lost. Keep the SMK powered; its operation may still be running. Check the local preview before retrying.');}
 }
 const response=await request('/api/smk/jobs',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,model:'smk-pro'})});
 const created=await response.json();if(!response.ok||!created.id)throw Error(created.error||'The local USB installer could not start.');
 let logged=0;
 for(;;){
  const reply=await request('/api/smk/jobs/'+encodeURIComponent(created.id),{cache:'no-store'});
  if(!reply.ok)throw Error('USB status could not be read. Keep the SMK powered; check the connection before retrying.');
  const state=await reply.json();
  for(const line of state.logs.slice(logged))log(line);logged=state.logs.length;
  reportProgress({phase:state.phase,...(state.phase==='transfer'?{bytes:state.bytes||0,total:state.total||1}:{})});
  if(state.state==='error')throw Error(state.error);
  if(state.state==='done')return state.identity;
  await new Promise(resolve=>setTimeout(resolve,300));
 }
}
