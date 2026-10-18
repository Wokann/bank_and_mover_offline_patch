#include "fakeServer/mover_checks.h"
#include "fakeServer/fakeserverapi.h"
#include "fakeServer/word_pattern.h"
#include "fakeServer/mover_expression.h"
#include "fakeServer/fake_servers.h"

/* The native response parser expects a status byte, big-endian slot count and
   decisions, followed by a fixed-size response tail. No client phase is encoded. */
/* 原版响应解析器读取状态字节、大端槽数及判定，最后是固定长度尾部；不编码客户端阶段。 */
int FsServer_CheckBoxReply(FsServer *server,uint32_t source,uint32_t count,
    FsMoverObserveSlot observe,void *user,uint8_t *body,uint32_t capacity,uint32_t *size)
{
    if (size) *size=0;
    if (!body || !size || !observe || count!=FS_MOVER_SLOT_COUNT ||
        capacity<FS_MOVER_REPLY_SIZE || (source!=FS_MOVER_GEN5 && source!=FS_MOVER_VC))
        return FS_INVALID;
    uint32_t applied=0;
    int configuration=FsServer_ReloadConfiguration(server,&applied);
    if (configuration!=FS_OK) return configuration;
    FsConfigError error;
    int result=FsConfiguration_LoadMover(server->storage,&server->configuration,source,&error);
    if (result!=FS_OK) goto finished;
    for (uint32_t i=0;i<FS_MOVER_REPLY_SIZE;i++) body[i]=0;
    body[2]=(uint8_t)count;
    for (uint32_t i=0;i<count;i++) {
        FsMoverCandidate candidate={0,0,0};
        FsMoverDetails details;
        for (uint32_t j=0;j<sizeof(details);j++) ((uint8_t *)&details)[j]=0;
        int status=observe(i,&candidate,&details,user);
        if (status!=FS_OK) { result=status;goto finished; }
        if (details.source && details.source!=source) { result=FS_INVALID;goto finished; }
        int32_t decision=FS_SLOT_REJECTED;
        status=FsServer_CheckPokemon(server,&candidate,details.source?&details:0,
            source==FS_MOVER_GEN5?649:255,&decision);
        if (status!=FS_OK) { result=status;goto finished; }
        uint32_t value=(uint32_t)decision,offset=3+4*i;
        body[offset]=(uint8_t)(value>>24); body[offset+1]=(uint8_t)(value>>16);
        body[offset+2]=(uint8_t)(value>>8); body[offset+3]=(uint8_t)value;
    }
    *size=FS_MOVER_REPLY_SIZE;
finished:
    /* The response contains decisions, not pointers into the policy. Return
       all rule/word/work allocations before the native SDK consumes it or
       allocates the next upload/download vector. This cleanup does not write. */
    /* 响应只含判定数值，不含词池指针；原 SDK 消费响应或分配下次上传／下载向量前
       归还全部规则、词池及匹配工作区；此清理步骤不写配置。 */
    FsConfiguration_Release(server->storage,&server->configuration);
    return result;
}

static int decodeText(const char *text,uint16_t *out,uint32_t capacity,uint32_t *length)
{
    uint32_t used=0;
    while (*text) {
        uint32_t value=(uint8_t)*text++,count=0,minimum=0;
        if (value>=0x80) {
            if (value>=0xc2 && value<=0xdf) { value&=31;count=1;minimum=0x80; }
            else if (value>=0xe0 && value<=0xef) { value&=15;count=2;minimum=0x800; }
            else if (value>=0xf0 && value<=0xf4) { value&=7;count=3;minimum=0x10000; }
            else return FS_INVALID;
            for (uint32_t i=0;i<count;i++) {
                uint32_t byte=(uint8_t)*text++;
                if ((byte&0xc0)!=0x80) return FS_INVALID;
                value=(value<<6)|(byte&63);
            }
            if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return FS_INVALID;
        }
        if (value<32 || value==0x7f) return FS_INVALID;
        uint32_t units=value>0xffff?2:1;
        if (used>UINT32_MAX-units) return FS_NO_MEMORY;
        if (out && (used>=capacity || units>=capacity-used)) return FS_INVALID;
        if (value>0xffff) {
            value-=0x10000;
            if (out) { out[used]=(uint16_t)(0xd800+(value>>10));out[used+1]=(uint16_t)(0xdc00+(value&1023)); }
        } else if (out) out[used]=(uint16_t)value;
        used+=units;
    }
    if (out) {
        if (used>=capacity) return FS_INVALID;
        out[used]=0;
    }
    *length=used;
    return used?FS_OK:FS_INVALID;
}
int FsMover_DecodeText(const char *text,uint16_t *out,uint32_t capacity)
{
    uint32_t length=0;
    return text && out && decodeText(text,out,capacity,&length)==FS_OK;
}

static uint16_t halfKana(uint16_t value)
{
    static const uint16_t kana[]={
        0x30f2,0x30a1,0x30a3,0x30a5,0x30a7,0x30a9,0x30e3,0x30e5,0x30e7,0x30c3,0x30fc,
        0x30a2,0x30a4,0x30a6,0x30a8,0x30aa,0x30ab,0x30ad,0x30af,0x30b1,0x30b3,
        0x30b5,0x30b7,0x30b9,0x30bb,0x30bd,0x30bf,0x30c1,0x30c4,0x30c6,0x30c8,
        0x30ca,0x30cb,0x30cc,0x30cd,0x30ce,0x30cf,0x30d2,0x30d5,0x30d8,0x30db,
        0x30de,0x30df,0x30e0,0x30e1,0x30e2,0x30e4,0x30e6,0x30e8,
        0x30e9,0x30ea,0x30eb,0x30ec,0x30ed,0x30ef,0x30f3};
    value=kana[value-0xff66];
    return value>=0x30a1 && value<=0x30f6?(uint16_t)(value-0x60):value;
}

static uint16_t voiced(uint16_t base,uint16_t mark)
{
    if (base>=0x306f && base<=0x307b && (base-0x306f)%3==0)
        return (uint16_t)(base+(mark==0x3099?1:2));
    if (mark!=0x3099) return 0;
    if (base==0x3046) return 0x3094;
    if ((base>=0x304b && base<=0x3053 && (base&1)) ||
        (base>=0x3055 && base<=0x305d && (base&1)) ||
        base==0x305f || base==0x3061 || base==0x3064 || base==0x3066 || base==0x3068 ||
        base==0x309d) return (uint16_t)(base+1);
    return 0;
}
static int validWordTag(uint32_t tag)
{
    uint32_t kind=tag&FS_MOVER_WORD_KIND_MASK;
    return !(tag&~252u) && (kind==FS_MOVER_WORD_TAG ||
        kind==FS_MOVER_ALLOW_NICKNAME_TAG || kind==FS_MOVER_ALLOW_OT_TAG);
}
int FsMover_WordSection(const char *name,uint32_t *tag)
{
    static const char *const names[]={"ngword","allow_nickname","allow_ot"};
    static const char *const regions[]={"JPN","USA","EUR","AUS","CHN","KOR","TWN"};
    if (!name || !tag) return 0;
    for (uint32_t kind=0;kind<3;kind++) {
        uint32_t i=0;
        while (names[kind][i] && names[kind][i]==name[i]) i++;
        if (names[kind][i] || (name[i] && name[i]!='.')) continue;
        *tag=kind==0?FS_MOVER_WORD_TAG:kind==1?FS_MOVER_ALLOW_NICKNAME_TAG:FS_MOVER_ALLOW_OT_TAG;
        if (!name[i]) return 1;
        const char *suffix=name+i+1;
        for (uint32_t region=0;region<7;region++) {
            uint32_t j=0;
            while (regions[region][j] && regions[region][j]==suffix[j]) j++;
            if (!regions[region][j] && !suffix[j]) {
                *tag|=(region+1u)<<FS_MOVER_WORD_REGION_SHIFT; return 1;
            }
        }
        return 0;
    }
    return 0;
}
int FsMover_WordHeader(const char *text)
{
    /* Retired names remain reserved: [words] must not become a character class. */
    /* 已删除的段名仍保留，不能把 [words] 当成字符集。 */
    static const char *const names[]={"global","rule","ngword","allow_nickname","allow_ot",
        "words","regex","allow","allow_regex"};
    if (!text || *text!='[') return 0;
    for (uint32_t n=0;n<sizeof(names)/sizeof(*names);n++) {
        uint32_t i=0;
        while (names[n][i] && names[n][i]==text[i+1]) i++;
        if (!names[n][i] && (text[i+1]==']' || text[i+1]=='.')) return 1;
    }
    return 0;
}
static uint32_t wordHash(const uint16_t *text,uint32_t length,uint32_t tag)
{
    uint32_t hash=2166136261u^tag;
    for (uint32_t i=0;i<length;i++) hash=(hash^text[i])*16777619u;
    return hash;
}
void FsMover_ReleaseWordIndex(const FsMoverPolicy *policy,FsMoverWordIndex *index)
{
    if (index->slots && policy->wordWork.memory.release) policy->wordWork.memory.release(index->slots);
    *index=(FsMoverWordIndex){0};
}
void FsMover_ReleaseWords(FsMoverPolicy *policy)
{
    if (!policy) return;
    if (policy->words && policy->wordWork.memory.release) policy->wordWork.memory.release(policy->words);
    policy->words=0;policy->wordBytes=0;policy->wordCapacity=0;policy->wordCount=0;
    for (uint32_t i=0;i<2;i++) { policy->wordSkipped[i]=0;policy->wordSkippedLine[i]=0; }
    FsWordPattern_Release(&policy->wordWork);
}
static int resizeIndex(FsMoverPolicy *policy,FsMoverWordIndex *index,uint32_t needed)
{
    uint32_t capacity=index->capacity?index->capacity:8;
    while (needed>=capacity-capacity/4u) {
        if (capacity>UINT32_MAX/8u) return FS_NO_MEMORY;
        capacity*=2;
    }
    if (capacity==index->capacity) return FS_OK;
    const FsMemory *memory=&policy->wordWork.memory;
    if (!memory->allocate || !memory->release) return FS_NO_MEMORY;
    uint32_t *slots=memory->allocate(capacity*sizeof(*slots));
    if (!slots) return FS_NO_MEMORY;
    for (uint32_t i=0;i<capacity;i++) slots[i]=0;
    uint32_t offset=0;
    for (uint32_t i=0;i<policy->wordCount;i++) {
        const FsMoverWord *word=FsMover_WordAt(policy,offset);
        uint32_t bucket=wordHash(word->text,word->length,word->tag&252u)&(capacity-1u);
        while (slots[bucket]) bucket=(bucket+1u)&(capacity-1u);
        slots[bucket]=offset+1u;offset+=FsMover_WordBytes(word->length);
    }
    if (index->slots) memory->release(index->slots);
    index->slots=slots;index->capacity=capacity;index->count=policy->wordCount;
    return FS_OK;
}
int FsMover_IndexWords(FsMoverPolicy *policy,FsMoverWordIndex *index)
{
    if (!index || !FsMover_ValidateWords(policy)) return FS_INVALID;
    FsMover_ReleaseWordIndex(policy,index);
    return resizeIndex(policy,index,policy->wordCount);
}
static int addWord(FsMoverPolicy *policy,FsMoverWordIndex *index,const char *text,uint32_t sources,uint32_t tag)
{
    if (!policy || !text || !sources || sources>FS_MOVER_ALL || !validWordTag(tag)) return FS_INVALID;
    FsWordWorkspace *work=&policy->wordWork;
    uint32_t length=0;
    int result=decodeText(text,0,0,&length);
    if (result!=FS_OK) return result;
    if (length>(UINT32_MAX-10u)/2u) return FS_NO_MEMORY;
    result=FsMemory_Reserve(&work->memory,&work->decoded,&work->decodedCapacity,length+1u,0,sizeof(uint16_t));
    if (result!=FS_OK) return result;
    uint16_t *decoded=work->decoded;
    result=decodeText(text,decoded,work->decodedCapacity,&length);
    if (result!=FS_OK) return result;
    if ((tag&FS_MOVER_WORD_KIND_MASK)==FS_MOVER_WORD_TAG) {
        result=FsWordPattern_Valid(work,decoded,length);
        if (result!=FS_OK) return result;
    }
    if (index) {
        if (policy->wordBytes>policy->wordCapacity || (policy->wordBytes && !policy->words)) return FS_INVALID;
    } else if (!FsMover_ValidateWords(policy)) return FS_INVALID;
    uint32_t offset=0,bucket=0;
    if (index) {
        if (index->count!=policy->wordCount) return FS_INVALID;
        if (index->capacity) {
            uint32_t *slots=index->slots;
            bucket=wordHash(decoded,length,tag)&(index->capacity-1u);
            while (slots[bucket]) {
                offset=slots[bucket]-1u;
                if (offset>=policy->wordBytes) return FS_INVALID;
                FsMoverWord *word=FsMover_WordAt(policy,offset);
                if (word->length==length && (word->tag&252u)==tag) {
                    uint32_t j=0;while (j<length && word->text[j]==decoded[j]) j++;
                    if (j==length) { word->tag|=sources;return FS_OK; }
                }
                bucket=(bucket+1u)&(index->capacity-1u);
            }
        }
        if (policy->wordCount==UINT32_MAX) return FS_NO_MEMORY;
        result=resizeIndex(policy,index,policy->wordCount+1u);
        if (result!=FS_OK) return result;
        uint32_t *slots=index->slots;
        bucket=wordHash(decoded,length,tag)&(index->capacity-1u);
        while (slots[bucket]) bucket=(bucket+1u)&(index->capacity-1u);
    } else for (uint32_t i=0;i<policy->wordCount;i++) {
        FsMoverWord *word=FsMover_WordAt(policy,offset);
        if (word->length==length && (word->tag&252u)==tag) {
            uint32_t j=0;while (j<length && word->text[j]==decoded[j]) j++;
            if (j==length) { word->tag|=sources;return FS_OK; }
        }
        offset+=FsMover_WordBytes(word->length);
    }
    uint32_t bytes=FsMover_WordBytes(length);
    if (bytes>UINT32_MAX-policy->wordBytes || policy->wordCount==UINT32_MAX) return FS_NO_MEMORY;
    result=FsMemory_Reserve(&work->memory,&policy->words,&policy->wordCapacity,
        policy->wordBytes+bytes,policy->wordBytes,1);
    if (result!=FS_OK) return result;
    offset=policy->wordBytes;
    FsMoverWord *word=FsMover_WordAt(policy,offset);
    word->length=length;word->tag=sources|tag;
    for (uint32_t i=0;i<length;i++) word->text[i]=decoded[i];
    if (length&1u) word->text[length]=0;
    policy->wordBytes+=bytes;policy->wordCount++;
    if (index) { ((uint32_t *)index->slots)[bucket]=offset+1u;index->count=policy->wordCount; }
    return FS_OK;
}
int FsMover_AddWord(FsMoverPolicy *policy,const char *text,uint32_t sources)
{ return addWord(policy,0,text,sources,FS_MOVER_WORD_TAG); }
int FsMover_AddIndexedWord(FsMoverPolicy *policy,FsMoverWordIndex *index,const char *text,uint32_t sources)
{ return index?addWord(policy,index,text,sources,FS_MOVER_WORD_TAG):FS_INVALID; }
int FsMover_AddIndexedNameRule(FsMoverPolicy *policy,FsMoverWordIndex *index,const char *text,uint32_t sources,uint32_t tag)
{ return index?addWord(policy,index,text,sources,tag):FS_INVALID; }

int FsMover_ValidateWords(const FsMoverPolicy *policy)
{
    if (!policy || policy->wordBytes>policy->wordCapacity || (policy->wordBytes && !policy->words)) return 0;
    uint32_t offset=0;
    for (uint32_t i=0;i<policy->wordCount;i++) {
        if (offset>policy->wordBytes || policy->wordBytes-offset<8u) return 0;
        const FsMoverWord *word=FsMover_WordAt(policy,offset);
        uint32_t length=word->length,tag=word->tag;
        if (!length || length>(UINT32_MAX-10u)/2u || !(tag&FS_MOVER_ALL) ||
            !validWordTag(tag&~FS_MOVER_ALL) || FsMover_WordBytes(length)>policy->wordBytes-offset) return 0;
        for (uint32_t j=0;j<length;j++) if (!word->text[j]) return 0;
        offset+=FsMover_WordBytes(length);
    }
    return offset==policy->wordBytes;
}

int FsMover_CheckRules(const FsMoverPolicy *policy,uint16_t species,const FsMoverDetails *p)
{
    if (!policy || policy->ruleCount>policy->ruleCapacity ||
        (policy->ruleCount && !policy->rules)) return 0;
    for (uint32_t i=0;i<policy->ruleCount;i++) {
        const FsMoverRule *rule=policy->rules+i;
        if (!rule->enabled) continue;
        if (!p) return 0;
        if (!(rule->sources&p->source)) continue;
        int applies=0;
        int matched=FsMoverExpression_Match(rule->condition,species,p,&applies);
        if (!rule->condition ||
            (rule->action==FS_MOVER_REJECT && matched) ||
            (rule->action==FS_MOVER_REQUIRE && applies && !matched)) return 0;
    }
    return 1;
}

typedef struct WordNameView {
    uint16_t text[FS_WORD_MATCH_UNITS+1];
    uint32_t begins,ends,length;
} WordNameView;
_Static_assert(2*FS_MOVER_NAME_UNITS<=FS_WORD_MATCH_UNITS && FS_WORD_MATCH_UNITS<32,
    "word views must hold lowercase expansion and fit the position mask");
static int space(uint16_t value) { return value==' ' || value==0x3000; }
#include "word_lower.inc"
static uint16_t lower(uint16_t value)
{
    if ((value>='A' && value<='Z') || (value>=0xc0 && value<=0xde && value!=0xd7))
        return (uint16_t)(value+32);
    if (value<=0xff) return value;
    uint32_t first=0,last=sizeof(lowerRanges)/sizeof(*lowerRanges);
    while (first<last) {
        uint32_t middle=first+(last-first)/2;
        const uint16_t *range=lowerRanges[middle];
        if (value<range[0]) last=middle;
        else if (value>range[1]) first=middle+1;
        else return range[3]==2 && ((value-range[0])&1)?value:(uint16_t)(range[2]+value-range[0]);
    }
    return value;
}
static void wordEdges(WordNameView *view)
{
    view->begins=1;
    view->ends=1u<<view->length;
    for (uint32_t i=0;i<view->length;i++) if (space(view->text[i])) {
        view->begins|=1u<<(i+1); view->ends|=1u<<i;
    }
}
static void appendView(WordNameView *view,uint16_t value,const WordNameView *original,
    uint32_t start,uint32_t end)
{
    uint32_t at=view->length++;
    view->text[at]=value;
    if (original->begins&(1u<<start)) view->begins|=1u<<at;
    if (original->ends&(1u<<end)) view->ends|=1u<<(at+1);
}
static uint32_t nameViews(const uint16_t *name,WordNameView views[4])
{
    uint32_t count=1;
    views[0]=(WordNameView){0};
    while (views[0].length<FS_MOVER_NAME_UNITS && name[views[0].length] &&
        name[views[0].length]!=0xffff) {
        views[0].text[views[0].length]=name[views[0].length]; views[0].length++;
    }
    wordEdges(views);
    WordNameView normalized={0};
    int different=0;
    for (uint32_t i=0;i<views[0].length;i++) {
        uint16_t value=name[i]; uint32_t end=i+1;
        if (value>=0xff9e && value<=0xff9f) { different=1; continue; }
        if ((value>=0xff10 && value<=0xff19) || (value>=0xff21 && value<=0xff3a) ||
            (value>=0xff41 && value<=0xff5a)) value-=0xfee0;
        if (value>=0xff66 && value<=0xff9d) {
            value=halfKana(value);
            if (i+1<views[0].length && (name[i+1]==0xff9e || name[i+1]==0xff9f)) {
                uint16_t combined=voiced(value,name[i+1]==0xff9e?0x3099:0x309a);
                if (combined) { value=combined; end++; }
            }
        }
        if (value>=0x3041 && value<=0x3096) value+=0x60;
        if (value==0x30a1 || value==0x30a3 || value==0x30a5 || value==0x30a7 || value==0x30a9 ||
            value==0x30c3 || value==0x30e3 || value==0x30e5 || value==0x30e7 || value==0x30ee) value++;
        if (value==0x30f5) value=0x30ab;
        if (value==0x30f6) value=0x30b1;
        uint16_t folded=lower(value);
        if (folded!=name[i] || value==0x130) different=1;
        appendView(&normalized,folded,views,i,end);
        /* Unicode's only unconditional BMP lowercase expansion: I-dot -> i + dot. */
        /* Unicode 唯一的无条件 BMP 小写展开：带点大写 I -> i + 组合点。 */
        if (value==0x130) appendView(&normalized,0x307,views,i,end);
    }
    if (different) views[count++]=normalized;
    uint32_t originals=count;
    for (uint32_t v=0;v<originals;v++) {
        const WordNameView *base=views+v;
        WordNameView compact={0}; int removed=0;
        for (uint32_t i=0;i<base->length;i++) {
            if (space(base->text[i])) { removed=1; continue; }
            uint32_t j=compact.length++;
            compact.text[j]=base->text[i];
            if (base->begins&(1u<<i)) compact.begins|=1u<<j;
            if (base->ends&(1u<<(i+1))) compact.ends|=1u<<(j+1);
        }
        if (removed && compact.length) views[count++]=compact;
    }
    return count;
}
static int wordMatch(FsMoverPolicy *policy,const uint16_t *name,uint32_t sources,
    uint32_t consoleRegion,uint32_t exceptionKind)
{
    if (!policy || !policy->wordCount) return 0;
    uint32_t nameLength=0;
    while (nameLength<FS_MOVER_NAME_UNITS && name[nameLength] && name[nameLength]!=0xffff) nameLength++;
    if (!nameLength) return 0;
    WordNameView views[4];uint32_t viewCount=0;
    /* Resolve exact exceptions before evaluating any patterns, including OOM.
       This makes whitelist priority independent of physical line order. */
    /* 先判定完整白名单再执行表达式，连内存不足也不会使优先级依赖词条位置。 */
    for (uint32_t pass=0;pass<2;pass++) {
        uint32_t offset=0;
        for (uint32_t w=0;w<policy->wordCount;w++) {
            const FsMoverWord *word=FsMover_WordAt(policy,offset);
            uint32_t tag=word->tag,size=word->length;offset+=FsMover_WordBytes(size);
            if (!(tag&sources)) continue;
            uint32_t region=(tag>>FS_MOVER_WORD_REGION_SHIFT)&7u;
            if (region && (consoleRegion>6 || consoleRegion+1u!=region)) continue;
            uint32_t kind=tag&FS_MOVER_WORD_KIND_MASK;
            if (!pass) {
                if (kind!=exceptionKind || size!=nameLength) continue;
                uint32_t i=0;while (i<size && name[i]==word->text[i]) i++;
                if (i==size) return 0;
                continue;
            }
            if (kind!=FS_MOVER_WORD_TAG) continue;
            if (!viewCount) viewCount=nameViews(name,views);
            for (uint32_t v=0;v<viewCount;v++) {
                int result=FsWordPattern_MatchBoundaries(&policy->wordWork,word->text,size,views[v].text,
                    views[v].length,views[v].begins,views[v].ends);
                if (result) return result;
            }
        }
    }
    return 0;
}
int FsMover_NameResult(FsMoverPolicy *policy,const FsMoverDetails *p,uint32_t consoleRegion)
{
    if (!p) return FS_NAME_UNCHANGED;
    uint32_t options=policy?policy->customOptions:0;
    int nicknameHan=FsMover_HasHan(p->nickname,FS_MOVER_NAME_UNITS);
    int checkNickname=p->nicknamed || ((options&FS_CUSTOM_ALLOW_FAN_TRANSLATION_CHN) && nicknameHan);
    /* Eligibility already checked Han; NGWord retains only its native
       name-correction role. Fan translation does not bypass word patterns. */
    /* 汉字拒绝已由宝可梦筛选处理；词表只请求改名，汉化不豁免普通敏感词。 */
    int nickname=checkNickname?
        wordMatch(policy,p->nickname,p->source,consoleRegion,FS_MOVER_ALLOW_NICKNAME_TAG):0;
    if (nickname<0) return nickname;
    int trainer=wordMatch(policy,p->trainer,p->source,consoleRegion,FS_MOVER_ALLOW_OT_TAG);
    if (trainer<0) return trainer;
    if (!nickname && !trainer) return FS_NAME_UNCHANGED;
    uint32_t base=p->source==FS_MOVER_GEN5?FS_NAME_FIX_NICKNAME:FS_NAME_FIX_VC_NICKNAME;
    return (int)(base+(trainer?(nickname?2u:1u):0u));
}
