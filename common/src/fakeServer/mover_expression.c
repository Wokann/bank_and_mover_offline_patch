#include "fakeServer/mover_expression.h"
#include "fakeServer/memory.h"
#include "fakeServer/numbers.h"

enum ExpressionField {
    EXPR_AND,EXPR_OR,EXPR_SPECIES,EXPR_FORM,EXPR_LEVEL,EXPR_MET_LEVEL,
    EXPR_SHINY,EXPR_TID,EXPR_SID,EXPR_LANGUAGE,EXPR_ORIGIN,EXPR_FATEFUL,EXPR_MOVE,
    EXPR_DUPLICATE_MOVES,EXPR_OT,EXPR_UNICODE_CHINESE
};
enum ExpressionComparison { EXPR_EQ,EXPR_NE,EXPR_LT,EXPR_LE,EXPR_GT,EXPR_GE };
enum { EXPR_OPEN=255 };
typedef struct ExpressionNode {
    uint32_t field,comparison;
    union { uint32_t number;uint16_t text[FS_MOVER_NAME_UNITS]; } value;
} ExpressionNode;
struct FsMoverExpression {
    uint32_t count,stackSize;
    ExpressionNode code[];
};
typedef struct ExpressionParser {
    FsMemory memory;
    char *cursor;
    ExpressionNode *nodes;
    uint8_t *operators;
    uint32_t count,capacity,operatorCount,operatorCapacity,depth,peak;
} ExpressionParser;

static void space(ExpressionParser *parser)
{
    while (*parser->cursor==' ' || *parser->cursor=='\t' ||
        *parser->cursor=='\r' || *parser->cursor=='\n') parser->cursor++;
}
static int word(const char *text,uint32_t size,const char *name)
{
    for (uint32_t i=0;i<size;i++) if (!name[i] || text[i]!=name[i]) return 0;
    return !name[size];
}
static int append(ExpressionParser *parser,const ExpressionNode *node)
{
    if (parser->count==UINT32_MAX) return FS_NO_MEMORY;
    int result=FsMemory_Reserve(&parser->memory,(void **)&parser->nodes,&parser->capacity,
        parser->count+1,parser->count,sizeof(*parser->nodes));
    if (result!=FS_OK) return result;
    parser->nodes[parser->count++]=*node;
    if (node->field<=EXPR_OR) {
        if (parser->depth<2) return FS_INVALID;
        parser->depth--;
    } else {
        parser->depth++;
        if (parser->depth>parser->peak) parser->peak=parser->depth;
    }
    return FS_OK;
}
static int push(ExpressionParser *parser,uint8_t operation)
{
    if (parser->operatorCount==UINT32_MAX) return FS_NO_MEMORY;
    int result=FsMemory_Reserve(&parser->memory,(void **)&parser->operators,&parser->operatorCapacity,
        parser->operatorCount+1,parser->operatorCount,1);
    if (result==FS_OK) parser->operators[parser->operatorCount++]=operation;
    return result;
}
static int pop(ExpressionParser *parser)
{
    ExpressionNode node={.field=parser->operators[--parser->operatorCount]};
    return node.field==EXPR_OPEN?FS_INVALID:append(parser,&node);
}
static int comparison(ExpressionParser *parser,uint32_t *operation)
{
    char first=*parser->cursor++;
    if (first=='=') {
        if (*parser->cursor!='=') return FS_INVALID;
        parser->cursor++;*operation=EXPR_EQ;
    } else if (first=='!') {
        if (*parser->cursor!='=') return FS_INVALID;
        parser->cursor++;*operation=EXPR_NE;
    } else if (first=='<' || first=='>') {
        int equal=*parser->cursor=='=';
        if (equal) parser->cursor++;
        *operation=first=='<'?(equal?EXPR_LE:EXPR_LT):(equal?EXPR_GE:EXPR_GT);
    } else return FS_INVALID;
    return FS_OK;
}
static int atom(ExpressionParser *parser)
{
    static const char *const fields[]={"species","form","level","met_level","shiny",
        "tid","sid","language","origin","fateful","move","duplicate_moves","ot","UnicodeChinese"};
    char *name=parser->cursor;
    while ((*parser->cursor>='a' && *parser->cursor<='z') ||
        (*parser->cursor>='A' && *parser->cursor<='Z') || *parser->cursor=='_') parser->cursor++;
    uint32_t size=(uint32_t)(parser->cursor-name),field=EXPR_SPECIES;
    for (;field<=EXPR_UNICODE_CHINESE;field++) if (word(name,size,fields[field-EXPR_SPECIES])) break;
    if (!size || field>EXPR_UNICODE_CHINESE) return FS_INVALID;
    ExpressionNode node={.field=field};space(parser);
    int result=comparison(parser,&node.comparison);
    if (result!=FS_OK) return result;
    space(parser);
    if (field==EXPR_OT) {
        if (node.comparison>EXPR_NE || *parser->cursor!='"') return FS_INVALID;
        char *text=++parser->cursor,*output=text;
        while (*parser->cursor && *parser->cursor!='"') {
            char value=*parser->cursor++;
            if (value=='\\') {
                value=*parser->cursor++;
                if (value!='"' && value!='\\') return FS_INVALID;
            }
            if ((uint8_t)value<32) return FS_INVALID;
            *output++=value;
        }
        if (*parser->cursor!='"') return FS_INVALID;
        parser->cursor++;*output=0;
        if (*text && !FsMover_DecodeText(text,node.value.text,FS_MOVER_NAME_UNITS)) return FS_INVALID;
    } else {
        char *text=parser->cursor;
        while ((*parser->cursor>='0' && *parser->cursor<='9') ||
            (*parser->cursor>='a' && *parser->cursor<='z')) parser->cursor++;
        size=(uint32_t)(parser->cursor-text);
        if (!size) return FS_INVALID;
        int boolean=field==EXPR_SHINY || field==EXPR_FATEFUL || field==EXPR_DUPLICATE_MOVES || field==EXPR_UNICODE_CHINESE;
        if (boolean && word(text,size,"true")) node.value.number=1;
        else if (boolean && word(text,size,"false")) node.value.number=0;
        else {
            char saved=*parser->cursor;*parser->cursor=0;
            int valid=FsParseU32(text,UINT32_MAX,&node.value.number);
            *parser->cursor=saved;if (!valid) return FS_INVALID;
        }
        if (field==EXPR_MOVE && node.comparison>EXPR_NE) return FS_INVALID;
    }
    return append(parser,&node);
}

int FsMoverExpression_Parse(const FsStorageIo *io,char *text,FsMoverExpression **out)
{
    if (!out) return FS_INVALID;
    *out=0;
    if (!io || !text || !io->allocate || !io->release) return FS_NO_MEMORY;
    ExpressionParser parser={.memory={io->allocate,io->release},.cursor=text};
    int result=FS_OK,operand=1;
    for (;;) {
        space(&parser);
        if (!*parser.cursor) break;
        if (operand) {
            if (*parser.cursor=='(') { parser.cursor++;result=push(&parser,EXPR_OPEN); }
            else { result=atom(&parser);operand=0; }
        } else if (*parser.cursor==')') {
            parser.cursor++;
            while (parser.operatorCount && parser.operators[parser.operatorCount-1]!=EXPR_OPEN) {
                result=pop(&parser);if (result!=FS_OK) break;
            }
            if (result==FS_OK && !parser.operatorCount) result=FS_INVALID;
            if (result==FS_OK) parser.operatorCount--;
        } else {
            uint8_t operation;
            if (parser.cursor[0]=='&' && parser.cursor[1]=='&') operation=EXPR_AND;
            else if (parser.cursor[0]=='|' && parser.cursor[1]=='|') operation=EXPR_OR;
            else { result=FS_INVALID;break; }
            parser.cursor+=2;
            while (parser.operatorCount && parser.operators[parser.operatorCount-1]!=EXPR_OPEN &&
                parser.operators[parser.operatorCount-1]<=operation) {
                result=pop(&parser);if (result!=FS_OK) break;
            }
            if (result==FS_OK) result=push(&parser,operation);
            operand=1;
        }
        if (result!=FS_OK) break;
    }
    if (result==FS_OK && operand) result=FS_INVALID;
    while (result==FS_OK && parser.operatorCount) result=pop(&parser);
    if (result==FS_OK && parser.depth!=1) result=FS_INVALID;
    if (result==FS_OK) {
        if (parser.peak>UINT32_MAX-sizeof(FsMoverExpression) ||
            parser.count>FsDivideU32(UINT32_MAX-sizeof(FsMoverExpression)-parser.peak,sizeof(ExpressionNode),0))
            result=FS_NO_MEMORY;
        else {
            uint32_t bytes=(uint32_t)sizeof(FsMoverExpression)+parser.count*(uint32_t)sizeof(ExpressionNode)+parser.peak;
            FsMoverExpression *expression=io->allocate(bytes);
            if (!expression) result=FS_NO_MEMORY;
            else {
                expression->count=parser.count;expression->stackSize=parser.peak;
                for (uint32_t i=0;i<parser.count;i++) expression->code[i]=parser.nodes[i];
                *out=expression;
            }
        }
    }
    if (parser.nodes) io->release(parser.nodes);
    if (parser.operators) io->release(parser.operators);
    return result;
}

static int compare(uint32_t left,uint32_t right,uint32_t operation)
{
    if (operation==EXPR_EQ) return left==right;
    if (operation==EXPR_NE) return left!=right;
    if (operation==EXPR_LT) return left<right;
    if (operation==EXPR_LE) return left<=right;
    if (operation==EXPR_GT) return left>right;
    return left>=right;
}
static int evaluate(const ExpressionNode *node,uint16_t species,const FsMoverDetails *p)
{
    uint32_t value=0,field=node->field;
    if (field==EXPR_OT) {
        int equal=1;
        for (uint32_t i=0;i<FS_MOVER_NAME_UNITS;i++) {
            uint16_t actual=p->trainer[i]==0xffff?0:p->trainer[i];
            if (actual!=node->value.text[i]) { equal=0;break; }
            if (!actual) break;
        }
        return node->comparison==EXPR_EQ?equal:!equal;
    }
    if (field==EXPR_MOVE) {
        int found=0;
        for (uint32_t i=0;i<4;i++) if (p->moves[i]==node->value.number) found=1;
        return node->comparison==EXPR_EQ?found:!found;
    }
    if (field==EXPR_SPECIES) value=species;
    else if (field==EXPR_FORM) value=p->form;
    else if (field==EXPR_LEVEL) value=p->level;
    else if (field==EXPR_MET_LEVEL) value=p->metLevel;
    else if (field==EXPR_SHINY) value=p->shiny;
    else if (field==EXPR_TID) value=p->tid;
    else if (field==EXPR_SID) value=p->sid;
    else if (field==EXPR_LANGUAGE) value=p->language;
    else if (field==EXPR_ORIGIN) value=p->origin;
    else if (field==EXPR_FATEFUL) value=p->fateful;
    else if (field==EXPR_UNICODE_CHINESE)
        value=FsMover_HasHan(p->nickname,FS_MOVER_NAME_UNITS) || FsMover_HasHan(p->trainer,FS_MOVER_NAME_UNITS);
    else if (field==EXPR_DUPLICATE_MOVES) {
        /* Only compare nonempty native slots. No new client check or SD read. */
        /* 只比较非空原生招式槽；不新增客户端判断或 SD 读取。 */
        for (uint32_t i=1;i<4;i++) for (uint32_t j=0;j<i;j++)
            if (p->moves[i] && p->moves[i]==p->moves[j]) value=1;
    }
    return compare(value,node->value.number,node->comparison);
}
int FsMoverExpression_Match(FsMoverExpression *expression,uint16_t species,
    const FsMoverDetails *p,int *applies)
{
    if (applies) *applies=0;
    if (!expression || !p) return 0;
    uint8_t *stack=(uint8_t *)(expression->code+expression->count);uint32_t used=0;
    for (uint32_t i=0;i<expression->count;i++) {
        const ExpressionNode *node=expression->code+i;
        if (node->field<=EXPR_OR) {
            if (used<2) return 0;
            uint8_t right=stack[--used],left=stack[used-1];
            stack[used-1]=node->field==EXPR_AND?(left&right):(left|right);
        } else {
            if (used>=expression->stackSize) return 0;
            int match=evaluate(node,species,p);
            stack[used++]=(uint8_t)((match?1:0)|((node->field!=EXPR_SPECIES || match)?2:0));
        }
    }
    if (used!=1) return 0;
    if (applies) *applies=(stack[0]&2)!=0;
    return (stack[0]&1)!=0;
}
