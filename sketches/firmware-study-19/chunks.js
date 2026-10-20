// English phrase heuristics for a review prototype, not a syntactic parser.
// Pure functions: the browser supplies measured width; no DOM or network here.
const ChunkReader=(()=>{
  const set=text=>new Set(text.split(' '));
  const articles=set('a an the this that these those my your his her its our their some any each every another no');
  const prepositions=set('in on at by for from to into onto through under over beneath beyond beside behind between among across around against of with without towards toward until before after');
  const conjunctions=set('and but or nor so yet because although while unless if when where');
  const pronouns=set('i you he she it we they someone something everyone everything nobody nothing there');
  const auxiliaries=set('am is are was were be been being have has had do does did can could will would shall should may might must');
  const adjectives=set('blue red green dark bright small little quiet open old new tall warm cold long short narrow last first empty brass wooden folded broad young useful real soft pale distant late same other confident previous single');
  const verbs=set('found find put take took opened open closed close left leave stood stand sat sit looked look watched watch smiled smile turned turn went go began begin read wrote write brought bring set get got gave give grew grow knew know heard hear saw see made make kept keep felt feel lay lie caught catch slept sleep came come ran run lost lose forgot remember tell told said say see let wore wear hung hang meant mean');
  const clean=(token='')=>token.toLocaleLowerCase('en').replace(/^[^\p{L}\p{N}]+|[^\p{L}\p{N}]+$/gu,'');
  const ends=token=>! /^(?:Mr|Mrs|Ms|Dr|Prof|St|vs)\.$/i.test(token)&&(/[.!?;:,…](?:["'”’»\])}]+)?$/.test(token)||/[—–]$/.test(token));
  const adjective=w=>adjectives.has(w)||/(?:ous|ful|less|ive|able|ible|ish)$/.test(w);
  const verb=w=>verbs.has(w)||/(?:ed|ing)$/.test(w);
  const content=w=>w&&!articles.has(w)&&!prepositions.has(w)&&!conjunctions.has(w)&&!pronouns.has(w)&&!auxiliaries.has(w);
  function bond(left,right,previous=''){
    if(ends(left))return -12;
    const a=clean(left),b=clean(right);
    if(articles.has(a))return 9;
    if(b==='can'&&['watering','tin','spray','paint','soup'].includes(a))return 9;
    if(auxiliaries.has(a))return 7;
    if(a==='not'||a==='never')return 6;
    if(pronouns.has(a)&&(verb(b)||auxiliaries.has(b)))return 7;
    if(prepositions.has(a))return 5;
    if(conjunctions.has(a))return 4;
    if(articles.has(clean(previous))&&content(a)&&content(b)&&!verb(b))return 9;
    if(adjective(a)&&content(b))return 7;
    if(prepositions.has(b)||conjunctions.has(b)||articles.has(b)||pronouns.has(b))return -6;
    return 0;
  }
  function score(tokens,start,end){
    const count=end-start,last=clean(tokens[end-1]);
    let value=count===1?-4:count===2?2:3;
    for(let i=start;i<end-1;i++)value+=bond(tokens[i],tokens[i+1],tokens[i-1]);
    if(end<tokens.length&&!ends(tokens[end-1]))value-=Math.max(0,bond(tokens[end-1],tokens[end],tokens[end-2]));
    if(!ends(tokens[end-1])&&(articles.has(last)||prepositions.has(last)||auxiliaries.has(last)||conjunctions.has(last)||last==='not'||last==='never'))value-=count===1?4:10;
    if(ends(tokens[end-1]))value+=4;
    return value;
  }
  function segment(tokens,{maxWords=3,fits=()=>true}={}){
    const size=tokens.length,cost=Array(size+1).fill(-Infinity),next=Array(size);cost[size]=0;
    // Paragraph-local lookahead avoids leaving an article stranded in the next frame.
    for(let start=size-1;start>=0;start--){
      for(let count=1;count<=maxWords&&start+count<=size;count++){
        const end=start+count;
        if(count>1&&(ends(tokens[end-2])||!fits(tokens.slice(start,end))))break;
        const value=score(tokens,start,end)+cost[end];
        if(value>cost[start]){cost[start]=value;next[start]=end;}
      }
    }
    const groups=[];
    for(let start=0;start<size;){const end=next[start]??start+1;groups.push({start,end,words:tokens.slice(start,end)});start=end;}
    return groups;
  }
  function duration(tokens,wpm,{longDelay=0,complexDelay=0,punctuationDelay=0}={}){
    const base=tokens.length*60000/wpm;
    // Preserve word-rate semantics: three words get three word intervals.
    const wordExtra=tokens.reduce((sum,w)=>sum+(clean(w).length>8?longDelay:0)+(clean(w).length>11?complexDelay:0),0);
    return base+wordExtra+(ends(tokens.at(-1)||'')?punctuationDelay:0);
  }
  return {segment,duration,ends,clean};
})();
if(typeof module!=='undefined')module.exports=ChunkReader;
