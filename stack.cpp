#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LOW_LEVEL  1
#define MID_LEVEL  2
#define HIGH_LEVEL 3

#define LOCATION __FILE__, __FUNCTION__, __LINE__
#define INCREASE_MULTIPLIER 3 / 2
#define DECREASE_MULTIPLIER 2

#define STACK_DEBUG HIGH_LEVEL

#ifndef STACK_DEBUG
    #define STACK_DEBUG LOW_LEVEL
#endif

#define StackCtor(stack)                           \
    Stack stack;                                   \
    stack.creationInfo = {LOCATION, #stack};       \
    StackCtor_(&stack);

#define CheckAllStack(stack)                                                                  \
    if (StackError error = IsAllOk(stack)) {                                                  \
        printf("Error %s, read full description in dump file\n", ErrorToString(error));       \
                                                                                              \
        stack->dumpInfo = {LOCATION, #stack};                                                 \
                                                                                              \
        StackDump(stack);                                                                     \
        assert(0 && "Verify failed");                                                         \
    }

typedef uint64_t canary;
typedef uint64_t hashValue;
typedef int StackElem;

const uint32_t POISON = 0xE2202;
const canary CANARY = 0xDEADBEEFDEADBEEF;
const uint32_t FREE_VALUE = 0xF2EE;
const uint32_t STACK_BEGINNING_CAPACITY = 50;
const uint32_t HASH_BASE = 257;
const char* STACK_ELEMENT_TYPE = "int";

FILE* LOG_FILE = fopen("log.txt", "a");

enum StackError {
    NO_ERROR = 0,
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    CAPACITY_NEGATIVE,
    DATA_NULL,
    STACK_FREE,
    STACK_NULL,
    CAPACITY_INFINITE,
    LEFT_STACK_CANARY_IRRUPTION,
    RIGHT_STACK_CANARY_IRRUPTION,
    LEFT_DATA_CANARY_IRRUPTION,
    RIGHT_DATA_CANARY_IRRUPTION,
    STACK_HASH_IRRUPTION,
    DATA_HASH_IRRUPTION
};

struct VarInfo {
    const char* file;
    const char* function;
    int line;

    const char* name;
};

struct Stack {
    canary canaryLeft;

    struct VarInfo creationInfo;
    struct VarInfo dumpInfo;
    int32_t size;
    int32_t capacity;
    uint8_t* data;

    canary canaryRight;
};

int32_t StackCtor_(Stack* stack);
int32_t StackDtor(Stack* stack);

int32_t StackPush(Stack* stack, StackElem value);
StackElem StackPop(Stack* stack);

StackError IsStackOk(Stack* stack);
StackError IsDataOk(Stack* stack);
StackError IsCapacityOk(Stack* stack);
StackError IsSizeOk(Stack* stack);
StackError IsAllOk(Stack* stack);

uint64_t GetHash(uint8_t* pointer, uint32_t size);
void WriteStackHash(Stack* stack);

int StackDump(Stack* stack, FILE* outstream = stderr);
const char* ErrorToString(StackError error);

uint8_t* StackIncrease(Stack* stack);
uint8_t* StackDecrease(Stack* stack);

uint64_t powllu(int32_t base, int32_t power);

int main() {
    StackCtor(intStack);

    StackPush(&intStack, 5);
    StackPush(&intStack, 12);
    StackPush(&intStack, 13);

    //intStack.dumpInfo = {LOCATION, "intStack"};
    //StackDump(&intStack);

    StackPush(&intStack, 13);

    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));

    StackDtor(&intStack);
}

int32_t StackCtor_(Stack* stack) {
    assert(stack != nullptr && "STACK_NULL");
    setvbuf(LOG_FILE, NULL, _IONBF, 0);
    fprintf(LOG_FILE, "-------------------------------\n Stack created\n");

    stack->canaryLeft = CANARY;

    stack->capacity = STACK_BEGINNING_CAPACITY;
    stack->size = 0;

    stack->data = (uint8_t*)calloc(stack->capacity * sizeof(StackElem) + 2 * sizeof(canary) + 2 * sizeof(hashValue), 1);
    assert(stack->data != nullptr && "DATA_NULL");
    fprintf(LOG_FILE, "Memory for stack data allocated\n");

    stack->canaryRight = CANARY;

    *(canary*)(stack->data) = CANARY;
    *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem)) = CANARY;

    fprintf(LOG_FILE, "Canaries are set\n");

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int32_t curIdx = 0; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + curIdx * sizeof(StackElem)) = POISON;
        }
        fprintf(LOG_FILE, "Poison values are set\n");

        WriteStackHash(stack);
        fprintf(LOG_FILE, "Hash is written\n");
    #endif

    return 0;
}

int32_t StackDtor(Stack* stack) {
    CheckAllStack(stack);

    fprintf(LOG_FILE, "Trying to destroy stack\n");

    free(stack->data);
    stack->data = (uint8_t*)FREE_VALUE;
    stack->size = -1;

    fprintf(LOG_FILE, "Stack successfully destroyed\n");
    return 0;
}

int32_t StackPush(Stack* stack, StackElem value) {
    CheckAllStack(stack);

    fprintf(LOG_FILE, "Trying to push element in stack\n");
    if (stack->size >= stack->capacity) stack->data = StackIncrease(stack);
    *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + (stack->size++) * sizeof(StackElem)) = value;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        WriteStackHash(stack);
        fprintf(LOG_FILE, "New hash is written after pushing\n");
    #endif

    CheckAllStack(stack);
    fprintf(LOG_FILE, "New element is pushed\n");
    return 0;
}

StackElem StackPop(Stack* stack) {
    CheckAllStack(stack);

    fprintf(LOG_FILE, "Trying to pop element from stack\n");
    if (stack->capacity >= DECREASE_MULTIPLIER * stack->size) StackDecrease(stack);
    assert(stack->size != 0 && "STACK_UNDERFLOW");

    StackElem popped = *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + (--stack->size) * sizeof(StackElem));
    *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + (stack->size)*sizeof(StackElem)) = POISON;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        WriteStackHash(stack);
        fprintf(LOG_FILE, "New hash is written after popping\n");
    #endif
    CheckAllStack(stack);
    return popped;
}

StackError IsStackOk(Stack* stack) {
    fprintf(LOG_FILE, "Starting stack structure check...\n");
    if (stack == nullptr)                 return STACK_NULL;

    #if (STACK_DEBUG >= MID_LEVEL)
        if (stack->canaryLeft != CANARY)  return LEFT_STACK_CANARY_IRRUPTION;
        if (stack->canaryRight != CANARY) return RIGHT_STACK_CANARY_IRRUPTION;
    #endif

    #if (STACK_DEBUG >= HIGH_LEVEL)
        if (GetHash((uint8_t*)stack + sizeof(canary), sizeof(Stack) - 2 * sizeof(canary)) != *(hashValue*)(stack->data + sizeof(canary))) return STACK_HASH_IRRUPTION;
    #endif

    fprintf(LOG_FILE, "Stack structure is successfully checked\n");
    return NO_ERROR;
}

StackError IsDataOk(Stack* stack) {
    fprintf(LOG_FILE, "Starting stack data check...\n");

    if (stack->data == nullptr)           return  DATA_NULL;
    if (stack->data == (uint8_t*)0xF2EE)  return STACK_FREE;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        if (GetHash(stack->data + sizeof(canary) + 2 * sizeof(hashValue), stack->capacity * sizeof(StackElem)) != *(hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue)))  return DATA_HASH_IRRUPTION;
    #endif

    #if (STACK_DEBUG >= MID_LEVEL)
        if (*(canary*)(stack->data) != CANARY) return LEFT_DATA_CANARY_IRRUPTION;
        if (*(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity*sizeof(StackElem)) != CANARY) return RIGHT_DATA_CANARY_IRRUPTION;
    #endif

    fprintf(LOG_FILE, "Stack data is successfully checked\n");
    return NO_ERROR;
}

StackError IsCapacityOk(Stack* stack) {
    fprintf(LOG_FILE, "Starting stack capacity check...\n");

    if (stack->capacity < 0)        return CAPACITY_NEGATIVE;
    if (!isfinite(stack->capacity)) return CAPACITY_INFINITE;

    fprintf(LOG_FILE, "Stack capacity is successfully checked\n");
    return NO_ERROR;
}

StackError IsSizeOk(Stack* stack) {
    fprintf(LOG_FILE, "Starting stack size check...\n");

    if (stack->size > stack->capacity)   return STACK_OVERFLOW;
    if (stack->size < 0)                 return STACK_UNDERFLOW;

    fprintf(LOG_FILE, "Stack size is successfully checked\n");
    return NO_ERROR;
}

StackError IsAllOk(Stack* stack) {
    fprintf(LOG_FILE, "Starting all stack check...\n");

    if (StackError error =  IsDataOk(stack))     return error;
    if (StackError error =  IsStackOk(stack))    return error;
    if (StackError error =  IsCapacityOk(stack)) return error;
    if (StackError error =  IsSizeOk(stack))     return error;

    fprintf(LOG_FILE, "All stack is successfully checked\n");
    return NO_ERROR;
}

uint64_t GetHash(uint8_t* pointer, uint32_t size) {
    fprintf(LOG_FILE, "Trying to get hash from %p with size %u bytes...\n", pointer, size);
    uint64_t hashSum = 0;

    for (uint32_t curByte = 0; curByte < size; curByte++) {
        hashSum += *(pointer + curByte) * powllu(HASH_BASE, curByte);
    }

    fprintf(LOG_FILE, "Hash from %p is gotten\n", pointer);
    return hashSum;
}

void WriteStackHash(Stack* stack) {
    fprintf(LOG_FILE, "Trying to get hash from stack structure...\n");
    *(hashValue*)(stack->data + sizeof(canary)) = GetHash((uint8_t*)stack + sizeof(canary), sizeof(Stack) - 2 * sizeof(canary));
    fprintf(LOG_FILE, "Hash from stack structure is gotten\n");

    fprintf(LOG_FILE, "Trying to get hash from stack data...\n");
    *(hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue)) = GetHash(stack->data + sizeof(canary) + 2 * sizeof(hashValue), stack->capacity * sizeof(StackElem));
    fprintf(LOG_FILE, "Hash from stack data is gotten\n");
}

int StackDump(Stack* stack, FILE* outstream) {
    setvbuf(outstream, NULL, _IONBF, 0);

    fprintf(outstream, "Dump from %s() at %s(%d) in stack called now \"%s\": IsStackOk() FAILED\n", stack->dumpInfo.function, stack->dumpInfo.file, stack->dumpInfo.line, stack->dumpInfo.name);
    fprintf(outstream, "stack <%s> [%p] (ok) \"%s\" from %s (%d), %s(): {\n", STACK_ELEMENT_TYPE, stack, stack->creationInfo.name, stack->creationInfo.file, stack->creationInfo.line, stack->creationInfo.function);
    fprintf(outstream, "size = %d (%s)\n", stack->size, ErrorToString(IsSizeOk(stack)));
    fprintf(outstream, "capacity = %d (%s)\n\n", stack->capacity, ErrorToString(IsCapacityOk(stack)));
    #if (STACK_DEBUG >= MID_LEVEL)
        fprintf(outstream, "Stack canaries:\n");
        fprintf(outstream, "    canaryLeft[%p] = %I64d (%s)\n", &stack->canaryLeft, stack->canaryLeft, (stack->canaryLeft == CANARY) ? "Ok" : "IRRUPTION");
        fprintf(outstream, "    canaryRight[%p] = %I64d (%s)\n", &stack->canaryRight, stack->canaryRight, (stack->canaryRight == CANARY) ? "Ok" : "IRRUPTION");
        fprintf(outstream, "Data canaries:\n");
        fprintf(outstream, "    canaryLeft[%p] = %I64d (%s)\n", stack->data, *(canary*)stack->data, (*(canary*)stack->data == CANARY) ? "Ok" : "IRRUPTION");
        fprintf(outstream, "    canaryRight[%p] = %I64d (%s)\n\n", stack->data + stack->capacity*sizeof(StackElem) + sizeof(canary), *(canary*)(stack->data + stack->capacity*sizeof(StackElem) + sizeof(canary)), (*(canary*)(stack->data + stack->capacity*sizeof(StackElem) + sizeof(canary)) == CANARY) ? "Ok" : "IRRUPTION");
    #endif

    #if (STACK_DEBUG >= HIGH_LEVEL)
        fprintf(outstream, "Stack hashes:\n");
        uint64_t curHash = GetHash((uint8_t*)stack + sizeof(canary), sizeof(Stack) - 2 * sizeof(canary));
        fprintf(outstream, "    Stored stack hash[%p] = %I64d\n", stack->data + sizeof(canary), *(hashValue*)(stack->data + sizeof(canary)));
        fprintf(outstream, "    Current stack hash = %I64d\n", curHash);
        fprintf(outstream, "    %s\n", (*(hashValue*)(stack->data + sizeof(canary)) == curHash) ? "(Hashes are equal)" : "(HASHES AREN'T EQUAL)");

        fprintf(outstream, "Data hashes:\n");
        curHash = GetHash(stack->data + sizeof(canary) + 2 * sizeof(hashValue), sizeof(StackElem) * stack->capacity);
        fprintf(outstream, "    Stored data hash[%p] = %I64d\n", stack->data + sizeof(canary) + sizeof(hashValue), *(hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue)));
        fprintf(outstream, "    Current data hash = %I64d\n", curHash);
        fprintf(outstream, "    %s\n\n", (*(hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue)) == curHash) ? "(Hashes are equal)" : "(HASHES AREN'T EQUAL)");
    #endif

    fprintf(outstream, "data[%p] (%s)\n", stack->data, ErrorToString(IsDataOk(stack)));
    #if (STACK_DEBUG >= HIGH_LEVEL)
        fprintf(outstream, "{\n");
        for(int32_t curIdx = 0; curIdx < stack->capacity; curIdx++) {
            StackElem* curElement = (StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + curIdx*sizeof(StackElem));
            if (curIdx < stack->size) {
                fprintf(outstream, "   *[%d][%p] = %d (%s)\n", curIdx, curElement, *curElement, (*curElement == POISON) ? "MAYBE POISON" : "OK");
            }
            else {
                fprintf(outstream, "   [%d][%p] = %d (%s)\n", curIdx, curElement, *curElement, (*curElement == POISON) ? "POISON" : "NOT POISON, BUT SHOULD BE");
            }
        }

        fprintf(outstream, "}\n");
    #endif

    return 0;
}

const char* ErrorToString(StackError error) {
    switch(error) {
        case NO_ERROR:                      return "Ok";
        case STACK_OVERFLOW:                return "Overflow";
        case STACK_UNDERFLOW:               return "Underflow";
        case CAPACITY_NEGATIVE:             return "Negative capacity";
        case DATA_NULL:                     return "Data is null";
        case STACK_FREE:                    return "Stack is already free";
        case STACK_NULL:                    return "Stack is null";
        case CAPACITY_INFINITE:             return "Capacity is infinite";
        case LEFT_STACK_CANARY_IRRUPTION:   return "Someone irrupted left stack canary";
        case RIGHT_STACK_CANARY_IRRUPTION:  return "Someone irrupted right stack canary";
        case LEFT_DATA_CANARY_IRRUPTION:    return "Someone irrupted left data canary";
        case RIGHT_DATA_CANARY_IRRUPTION:   return "Someone irrupted right data canary";
        case STACK_HASH_IRRUPTION:          return "Someone irrupted stack";
        case DATA_HASH_IRRUPTION:           return "Someone irrupted data";

        default:                            return "Unknown error";
    }
}

uint8_t* StackIncrease(Stack* stack) {
    fprintf(LOG_FILE, "Trying to increase memory for stack...\n");
    CheckAllStack(stack);

    canary cellCanary = *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity*sizeof(StackElem)) = CANARY;
    *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity*sizeof(StackElem)) = 0;

    uint8_t* newPointer = (uint8_t*)realloc(stack->data, 2 * sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem) * INCREASE_MULTIPLIER + 1);
    assert(newPointer != nullptr && "MEMORY_INCREASE_ERR");
    stack->capacity = stack->capacity * INCREASE_MULTIPLIER + 1;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int32_t curIdx = stack->size + 1; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + curIdx*sizeof(StackElem)) = POISON;
        }
    #endif

    *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity*sizeof(StackElem)) = cellCanary;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        WriteStackHash(stack);
        fprintf(LOG_FILE, "New hash is written after memory increasing\n");
    #endif

    CheckAllStack(stack);

    fprintf(LOG_FILE, "Memory is successfully increased \n");
    return newPointer;
}

uint8_t* StackDecrease(Stack* stack) {
    fprintf(LOG_FILE, "Trying to decrease memory for stack...\n");
    CheckAllStack(stack);

    canary cellCanary = *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem)) = CANARY;
    *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem)) = 0;

    uint8_t* newPointer = (uint8_t*)realloc(stack->data, 2 * sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity*sizeof(StackElem) / DECREASE_MULTIPLIER);
    assert(newPointer != nullptr && "MEMORY_DECREASE_ERROR");
    stack->capacity /= DECREASE_MULTIPLIER;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int32_t curIdx = stack->size + 1; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + curIdx * sizeof(StackElem)) = POISON;
        }
    #endif

    *(canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem)) = cellCanary;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        WriteStackHash(stack);
        fprintf(LOG_FILE, "New hash is written after memory decreasing\n");
    #endif

    CheckAllStack(stack);

    fprintf(LOG_FILE, "Memory is successfully decreased \n");
    return newPointer;
}

uint64_t powllu(int32_t base, int32_t power) {
    uint64_t result = 1;

    for (int32_t curMulti = 0; curMulti < power; curMulti++) {
        result *= base;
    }

    return result;
}
