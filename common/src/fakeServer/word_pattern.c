#include "fakeServer/word_pattern.h"

#define BAD_NODE UINT32_MAX
enum { LITERAL,ANY,CLASS,BEGIN,END,DIGIT,WORD,SPACE,NOT_DIGIT,NOT_WORD,NOT_SPACE,EMPTY,
    JOIN,EITHER,OPTIONAL,STAR,PLUS,REPEAT };
typedef struct PatternNode {
    uint32_t value,left,right;
    uint8_t kind,nullable;
    uint16_t reserved;
} PatternNode;
typedef struct PatternFrame { uint32_t sequence,alternatives; } PatternFrame;
typedef struct Pattern {
    FsWordWorkspace *work;
    const uint16_t *text;
    uint32_t length,position,count,depth,begin,end;
    int error;
} Pattern;

void FsWordPattern_Release(FsWordWorkspace *work)
{
    if (!work) return;
    void *buffers[]={work->nodes,work->frames,work->ends,work->decoded};
    if (work->memory.release) for (uint32_t i=0;i<4;i++) if (buffers[i]) work->memory.release(buffers[i]);
    FsMemory memory=work->memory;
    *work=(FsWordWorkspace){.memory=memory};
}
static uint32_t node(Pattern *p,uint32_t kind,uint32_t value,uint32_t left,uint32_t right)
{
    if (p->error) return BAD_NODE;
    if ((kind==JOIN || kind==EITHER) && (left==BAD_NODE || right==BAD_NODE)) return BAD_NODE;
    if (kind>=OPTIONAL && left==BAD_NODE) return BAD_NODE;
    if (p->count==UINT32_MAX) { p->error=FS_NO_MEMORY;return BAD_NODE; }
    p->error=FsMemory_Reserve(&p->work->memory,&p->work->nodes,&p->work->nodeCapacity,
        p->count+1,p->count,sizeof(PatternNode));
    if (p->error) return BAD_NODE;
    PatternNode *nodes=p->work->nodes;
    uint32_t nullable=kind==BEGIN || kind==END || kind==EMPTY || kind==OPTIONAL || kind==STAR;
    if (kind==JOIN) nullable=nodes[left].nullable && nodes[right].nullable;
    else if (kind==EITHER) nullable=nodes[left].nullable || nodes[right].nullable;
    else if (kind==PLUS) nullable=nodes[left].nullable;
    else if (kind==REPEAT) nullable=!(value&255u) || nodes[left].nullable;
    uint32_t result=p->count++;
    nodes[result]=(PatternNode){value,left,right,(uint8_t)kind,(uint8_t)nullable,0};
    return result;
}
static int special(uint16_t value)
{
    return value=='.' || value=='^' || value=='$' || value=='[' || value==']' ||
        value=='(' || value==')' || value=='|' || value=='*' || value=='+' ||
        value=='?' || value=='{' || value=='}' || value=='\\' || value=='-';
}
static int escaped(const uint16_t *text,uint32_t position)
{
    uint32_t count=0;
    while (position && text[--position]=='\\') count++;
    return count&1u;
}
/* Outside markers constrain every alternative. Escaped tokens remain literal. */
/* 外侧标记约束所有分支，转义后的标记仍为文字。 */
static void edges(const uint16_t *text,uint32_t length,uint32_t *first,uint32_t *last,
    uint32_t *begin,uint32_t *end)
{
    *first=0;*last=length;*begin=0;*end=0;
    if (text[0]=='^') { *begin=1;*first=1; }
    else if (length>=2 && text[0]=='.' && text[1]=='*') *first=length>=3 && text[2]=='?'?3:2;
    if (*last>*first && text[*last-1]=='$' && !escaped(text,*last-1)) { *end=1;(*last)--; }
    else if (*last-*first>=3 && text[*last-3]=='.' && text[*last-2]=='*' &&
        text[*last-1]=='?' && !escaped(text,*last-3)) *last-=3;
    else if (*last-*first>=2 && text[*last-2]=='.' && text[*last-1]=='*' &&
        !escaped(text,*last-2)) *last-=2;
}
static int hex(uint16_t value)
{
    if (value>='0' && value<='9') return value-'0';
    if (value>='a' && value<='f') return value-'a'+10;
    return value>='A' && value<='F'?value-'A'+10:-1;
}
static int unit(const uint16_t *text,uint32_t length,uint32_t *position,uint16_t *value,uint32_t *kind)
{
    if (*position>=length) return 0;
    *kind=LITERAL;*value=text[(*position)++];
    if (*value!='\\') return 1;
    if (*position>=length) return 0;
    *value=text[(*position)++];
    switch (*value) {
    case 'd': *kind=DIGIT;break;
    case 'D': *kind=NOT_DIGIT;break;
    case 'w': *kind=WORD;break;
    case 'W': *kind=NOT_WORD;break;
    case 's': *kind=SPACE;break;
    case 'S': *kind=NOT_SPACE;break;
    case 't': *value=9;break;
    case 'n': *value=10;break;
    case 'v': *value=11;break;
    case 'f': *value=12;break;
    case 'r': *value=13;break;
    case 'x': case 'u': {
        uint32_t count=*value=='x'?2:4,decoded=0;
        if (count>length-*position) return 0;
        for (uint32_t i=0;i<count;i++) {
            int digit=hex(text[(*position)++]);
            if (digit<0) return 0;
            decoded=(decoded<<4)|(uint32_t)digit;
        }
        *value=(uint16_t)decoded;break;
    }
    default:
        if (!special(*value) && *value!='/' && *value!='#' && *value!=';' && *value!=' ') return 0;
    }
    return 1;
}
static int repetitions(Pattern *p,uint32_t *count)
{
    if (p->position==p->length || p->text[p->position]<'0' || p->text[p->position]>'9') return 0;
    *count=0;
    while (p->position<p->length && p->text[p->position]>='0' && p->text[p->position]<='9') {
        uint32_t digit=p->text[p->position++]-'0';
        if (*count>429496729u || (*count==429496729u && digit>5)) return 0;
        *count=*count*10+digit;
    }
    return 1;
}
static uint32_t repeat(Pattern *p,uint32_t result,int quantifiable)
{
    if (result==BAD_NODE || p->position==p->length) return result;
    uint16_t token=p->text[p->position];
    if (token=='?' || token=='*' || token=='+') {
        if (!quantifiable) return BAD_NODE;
        p->position++;
        result=node(p,token=='?'?OPTIONAL:token=='*'?STAR:PLUS,0,result,0);
    } else if (token=='{') {
        uint32_t minimum,maximum;
        if (!quantifiable) return BAD_NODE;
        p->position++;
        if (!repetitions(p,&minimum)) return BAD_NODE;
        maximum=minimum;
        if (p->position<p->length && p->text[p->position]==',') {
            p->position++;
            if (p->position<p->length && p->text[p->position]=='}') maximum=UINT32_MAX;
            else if (!repetitions(p,&maximum)) return BAD_NODE;
        }
        if (minimum>maximum || p->position==p->length || p->text[p->position++]!='}') return BAD_NODE;
        /* Equivalent finite-position closure, not a pattern/repetition limit. */
        /* 有限名字位置集合的等价闭包，不是表达式或重复次数上限。 */
        if (minimum>FS_WORD_MATCH_UNITS+1) minimum=FS_WORD_MATCH_UNITS+1;
        if (maximum>FS_WORD_MATCH_UNITS+1) maximum=FS_WORD_MATCH_UNITS+1;
        result=node(p,REPEAT,minimum|(maximum<<8),result,0);
    } else return result;
    if (p->position<p->length && p->text[p->position]=='?') p->position++;
    return result;
}
static uint32_t atom(Pattern *p)
{
    uint16_t value=p->text[p->position++];
    int quantifiable=value!='^' && value!='$';
    uint32_t result=BAD_NODE;
    if (value=='[') {
        uint32_t start=p->position,units=0;
        if (p->position<p->length && p->text[p->position]=='^') p->position++;
        while (p->position<p->length && p->text[p->position]!=']') {
            uint16_t first,last;uint32_t firstKind,lastKind;
            if (!unit(p->text,p->length,&p->position,&first,&firstKind)) return BAD_NODE;
            if (p->length-p->position>=2 && p->text[p->position]=='-' && p->text[p->position+1]!=']') {
                p->position++;
                if (!unit(p->text,p->length,&p->position,&last,&lastKind) ||
                    firstKind!=LITERAL || lastKind!=LITERAL || first>last) return BAD_NODE;
            }
            units++;
        }
        if (!units || p->position==p->length) return BAD_NODE;
        result=node(p,CLASS,start,0,p->position++);
    } else if (value=='\\') {
        uint32_t kind;p->position--;
        if (!unit(p->text,p->length,&p->position,&value,&kind)) return BAD_NODE;
        result=node(p,kind,value,0,0);
    } else if (value=='.' || value=='^' || value=='$')
        result=node(p,value=='.'?ANY:value=='^'?BEGIN:END,0,0,0);
    else if (!special(value) || value=='-') result=node(p,LITERAL,value,0,0);
    return repeat(p,result,quantifiable);
}
static uint32_t finish(Pattern *p)
{
    PatternFrame *frame=(PatternFrame *)p->work->frames+p->depth-1;
    uint32_t sequence=frame->sequence;
    if (sequence==BAD_NODE) sequence=node(p,EMPTY,0,0,0);
    return frame->alternatives==BAD_NODE?sequence:node(p,EITHER,0,frame->alternatives,sequence);
}
static int push(Pattern *p)
{
    if (p->depth==UINT32_MAX) return FS_NO_MEMORY;
    int result=FsMemory_Reserve(&p->work->memory,&p->work->frames,&p->work->frameCapacity,
        p->depth+1,p->depth,sizeof(PatternFrame));
    if (result!=FS_OK) return result;
    ((PatternFrame *)p->work->frames)[p->depth++]=(PatternFrame){BAD_NODE,BAD_NODE};
    return FS_OK;
}
static uint32_t compile(Pattern *p,FsWordWorkspace *work,const uint16_t *text,uint32_t length)
{
    *p=(Pattern){.work=work};
    if (!work || !text || !length) return BAD_NODE;
    uint32_t first,last;
    edges(text,length,&first,&last,&p->begin,&p->end);
    p->text=text+first;p->length=last-first;
    p->error=push(p);
    while (!p->error && p->position<p->length) {
        uint16_t token=p->text[p->position];
        if (token=='(') {
            p->position++;
            if (p->length-p->position>=2 && p->text[p->position]=='?' && p->text[p->position+1]==':') p->position+=2;
            p->error=push(p);continue;
        }
        if (token=='|') {
            uint32_t alternative=finish(p);
            if (alternative==BAD_NODE) return BAD_NODE;
            PatternFrame *frame=(PatternFrame *)work->frames+p->depth-1;
            frame->alternatives=alternative;frame->sequence=BAD_NODE;p->position++;continue;
        }
        uint32_t next;
        if (token==')') {
            if (p->depth==1) return BAD_NODE;
            next=finish(p);p->depth--;p->position++;next=repeat(p,next,1);
        } else next=atom(p);
        if (next==BAD_NODE) return BAD_NODE;
        PatternFrame *frame=(PatternFrame *)work->frames+p->depth-1;
        frame->sequence=frame->sequence==BAD_NODE?next:node(p,JOIN,0,frame->sequence,next);
        if (frame->sequence==BAD_NODE) return BAD_NODE;
    }
    return !p->error && p->depth==1?finish(p):BAD_NODE;
}
static int spaceUnit(uint16_t value)
{
    return (value>=9 && value<=13) || value==' ' || value==0xa0 || value==0x1680 ||
        (value>=0x2000 && value<=0x200a) || value==0x2028 || value==0x2029 ||
        value==0x202f || value==0x205f || value==0x3000 || value==0xfeff;
}
static int wordUnit(uint16_t value)
{
    return (value>='a' && value<='z') || (value>='A' && value<='Z') ||
        (value>='0' && value<='9') || value=='_';
}
static int character(uint32_t kind,uint16_t wanted,uint16_t value)
{
    int digit=value>='0' && value<='9';
    return (kind==LITERAL && value==wanted) ||
        (kind==ANY && value!=10 && value!=13 && value!=0x2028 && value!=0x2029) ||
        (kind==DIGIT && digit) || (kind==NOT_DIGIT && !digit) ||
        (kind==WORD && wordUnit(value)) || (kind==NOT_WORD && !wordUnit(value)) ||
        (kind==SPACE && spaceUnit(value)) || (kind==NOT_SPACE && !spaceUnit(value));
}
static int classMatch(const Pattern *p,const PatternNode *n,uint16_t value)
{
    uint32_t position=n->value;
    int inverted=p->text[position]=='^',matched=0;
    if (inverted) position++;
    while (position<n->right) {
        uint16_t first,last;uint32_t kind,lastKind;
        if (!unit(p->text,n->right,&position,&first,&kind)) return 0;
        last=first;
        if (n->right-position>=2 && p->text[position]=='-') {
            position++;
            if (!unit(p->text,n->right,&position,&last,&lastKind)) return 0;
            if (value>=first && value<=last) matched=1;
        } else if (character(kind,first,value)) matched=1;
    }
    return inverted?!matched:matched;
}
static int evaluate(Pattern *p,uint32_t root,const uint16_t *name,uint32_t length,uint32_t begins,uint32_t finishes)
{
    uint32_t columns=length+1;
    if (p->count>UINT32_MAX/columns) return FS_NO_MEMORY;
    int result=FsMemory_Reserve(&p->work->memory,&p->work->ends,&p->work->endCapacity,p->count*columns,0,sizeof(uint32_t));
    if (result!=FS_OK) return result;
    uint32_t *ends=p->work->ends;const PatternNode *nodes=p->work->nodes;
#define CELL(row,col) ends[(row)*columns+(col)]
    for (uint32_t i=0;i<p->count;i++) for (uint32_t start=0;start<=length;start++) {
        const PatternNode *n=nodes+i;uint32_t positions=0;
        if (n->kind==JOIN) {
            uint32_t middle=CELL(n->left,start);
            for (uint32_t j=start;j<=length;j++) if (middle&(1u<<j)) positions|=CELL(n->right,j);
        } else if (n->kind==EITHER) positions=CELL(n->left,start)|CELL(n->right,start);
        else if (n->kind==REPEAT) {
            uint32_t minimum=n->value&255u,maximum=n->value>>8,reachable=1u<<start;
            if (!minimum) positions=reachable;
            for (uint32_t count=1;count<=maximum;count++) {
                uint32_t next=0;
                for (uint32_t j=start;j<=length;j++) if (reachable&(1u<<j)) next|=CELL(n->left,j);
                if (count>=minimum) positions|=next;
                if (next==reachable) { positions|=next;break; }
                if (!(reachable=next)) break;
            }
        } else if (n->kind>=OPTIONAL) {
            positions=CELL(n->left,start);
            if (n->kind!=PLUS) positions|=1u<<start;
            if (n->kind!=OPTIONAL) {
                uint32_t previous;
                do {
                    previous=positions;
                    for (uint32_t j=start;j<=length;j++) if (positions&(1u<<j)) positions|=CELL(n->left,j);
                } while (previous!=positions);
            }
        } else if (n->kind==EMPTY) positions=1u<<start;
        else if (n->kind==BEGIN) positions=start==0?1:0;
        else if (n->kind==END) positions=start==length?1u<<start:0;
        else if (start<length && (n->kind==CLASS?classMatch(p,n,name[start]):character(n->kind,(uint16_t)n->value,name[start])))
            positions=1u<<(start+1);
        CELL(i,start)=positions;
    }
    for (uint32_t start=0;start<=length;start++) {
        if (p->begin && !(begins&(1u<<start))) continue;
        uint32_t positions=CELL(root,start);
        if (p->end) positions&=finishes;
        if (positions) return 1;
    }
#undef CELL
    return 0;
}
typedef struct LiteralPattern {
    const uint16_t *text;
    uint32_t first,last,length,begin,end;
} LiteralPattern;
static int literalPattern(const uint16_t *text,uint32_t length,LiteralPattern *p)
{
    *p=(LiteralPattern){.text=text};
    if (!text || !length) return 0;
    edges(text,length,&p->first,&p->last,&p->begin,&p->end);
    for (uint32_t i=p->first;i<p->last;) {
        uint16_t value=text[i];uint32_t kind;
        if (value!='\\' && special(value) && value!='-') return 0;
        if (!unit(text,p->last,&i,&value,&kind) || kind!=LITERAL) return 0;
        p->length++;
    }
    return 1;
}
static int literalMatch(const LiteralPattern *p,const uint16_t *name,uint32_t units,uint32_t begins,uint32_t finishes)
{
    if (p->length>units) return 0;
    for (uint32_t start=0;start<=units-p->length;start++) {
        uint32_t end=start+p->length;
        if ((p->begin && !(begins&(1u<<start))) || (p->end && !(finishes&(1u<<end)))) continue;
        uint32_t i=p->first,j=0;
        while (i<p->last) {
            uint16_t value;uint32_t kind;
            if (!unit(p->text,p->last,&i,&value,&kind) || name[start+j]!=value) break;
            j++;
        }
        if (j==p->length) return 1;
    }
    return 0;
}
int FsWordPattern_Valid(FsWordWorkspace *work,const uint16_t *text,uint32_t length)
{
    LiteralPattern literal;
    if (literalPattern(text,length,&literal)) return literal.length?FS_OK:FS_INVALID;
    Pattern p;uint32_t root=compile(&p,work,text,length);
    if (p.error) return p.error;
    return root!=BAD_NODE && !((PatternNode *)work->nodes)[root].nullable?FS_OK:FS_INVALID;
}
int FsWordPattern_MatchBoundaries(FsWordWorkspace *work,const uint16_t *text,uint32_t length,
    const uint16_t *name,uint32_t units,uint32_t begins,uint32_t finishes)
{
    if (!name || units>FS_WORD_MATCH_UNITS) return FS_INVALID;
    LiteralPattern literal;
    if (literalPattern(text,length,&literal)) return literalMatch(&literal,name,units,begins,finishes);
    Pattern p;uint32_t root=compile(&p,work,text,length);
    if (p.error) return p.error;
    return root!=BAD_NODE?evaluate(&p,root,name,units,begins,finishes):FS_INVALID;
}
