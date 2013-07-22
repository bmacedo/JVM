#include <stdlib.h>
#include "Controller.h"
#include "AuxiliarFunctions.h"
#include "InstructionManager.h"

int emptyStack(OperandStack *top) {
	return top == NULL ;
}

int countSlots(OperandStack* p1,int argNumber) {
    OperandStack* p2;
    Operand operand;
    int i, slots = 0;

    stackInit(&p2);
    for (i = 0; i < argNumber; i++, slots++) {
        operand = popOperand(&p1);
        pushOperand(&p2, operand);
        if (operand.type32_64 == CAT2) slots++;
    }
    for (i = 0; i < argNumber; i++) {
        pushOperand(&p1, popOperand(&p2));
    }
    return slots;
}

void pushOperand(OperandStack **topAddress, Operand operandToPush) {
	OperandStack *p1;
	p1 = malloc(sizeof(OperandStack));
	p1->operand = operandToPush;
	p1->nextOperand = *topAddress;
	*topAddress = p1;
}

void stackInit(OperandStack **stackAddress) {
	*stackAddress = NULL;
}

Operand popOperand(OperandStack **topAddress) {
	OperandStack *p1;
	Operand operand;
	if (!emptyStack(*topAddress)) {
		operand = (*topAddress)->operand;
		p1 = *topAddress;
		*topAddress = (*topAddress)->nextOperand;
		free(p1);
	} else {
		printf("A pilha esta vazia\n");
		exit(1);
	}
	return operand;
}

int emptyStackFrame(StackFrame *topFrame) {
	return topFrame == NULL ;
}

void frameStackInit(StackFrame **topFrame) {
	*topFrame = NULL;
}

void pushFrame(StackFrame **topFrameAddress) {
	StackFrame *p1;
	p1 = malloc(sizeof(StackFrame));
	p1->nextFrame = *topFrameAddress;
	*topFrameAddress = p1;
}

void popFrame(StackFrame **topFrameAddress) {
	StackFrame *p1;
	if (!emptyStackFrame(*topFrameAddress)) {
		p1 = *topFrameAddress;
		*topFrameAddress = (*topFrameAddress)->nextFrame;
		free(p1);
	} else {
		printf("A pilha esta vazia\n");
		exit(1);
	}
}

void frameInit(ClassList *init, ClassFile classfile, StackFrame *stackFrame, char* methodName, char* descriptor) {
	ClassFile* classfile_aux;
	MethodInfo* method;
	AttributeInfo methodCode;
	u2 superClassIndex;
	u1 *superClassName;
	int i;

	method = NULL;
	classfile_aux = &classfile;
	superClassIndex = classfile_aux->superClass;

	/* Procura o metodo pelo nome e se n�o encontrar, procura na super classe */
	while (classfile_aux != NULL && (method = getMethodByName(*classfile_aux, methodName, descriptor)) == NULL && superClassIndex != 0) {

        superClassName = getUTF8(classfile_aux->constantPool, classfile_aux->constantPool[superClassIndex].info.ClassInfo.nameIndex);

		classfile_aux = getClassFileByName(init, superClassName);
		if (classfile_aux != NULL ) {
			superClassIndex = classfile_aux->superClass;
		}
	}

	if (method == NULL ) {
		printf("Metodo nao encontrado >> %s %s\n", methodName, descriptor);
		exit(1);
	}

	for (i = 0; i < method->attributesCount; i++) {
		if (strcmp(getUTF8(classfile_aux->constantPool, method->attributes[i].attributeNameIndex), "Code")	== 0) {
			methodCode = method->attributes[i];
			break;
		}
	}

    /* belongingClass do frame recebe o constant_pool do classFile */
	stackFrame->frame->execEnvir->belongingClass = classfile_aux->constantPool;
	/* inicializa a pilha de operandos */
	stackInit(&(stackFrame->frame->topOperand));
	/* seta o opcode do m�todo que vai ser executado */
	stackFrame->frame->execEnvir->currOpcode = methodCode.AttributeType.CodeAttribute.code;
	/* inicializa vetor de vari�veis locais */
	stackFrame->frame->localVarArray = malloc(methodCode.AttributeType.CodeAttribute.maxLocals * sizeof(int));
	/* seta o pc */
	stackFrame->frame->execEnvir->pc = stackFrame->frame->execEnvir->currOpcode;
}


void initClassList(ClassList** head) {
    *head = NULL;
}

void initStaticFields(ClassList* cList, ClassFile cFile) {
    int i, j = 0;
    // Conta os campos estaticos
	for (i = 0; i < cFile.fieldsCount; i++) {
		if ((cFile.fields[i].accessFlags & ACC_STATIC) != 0) {
			cList->staticFieldsCount++;
		}
	}
	//Aloca o vetor de campos estaticos
	cList->staticFields = malloc(sizeof(Field) * cList->staticFieldsCount);
	// Insere os valores de nome e descritor no vetor e inicializa com 0
	for (i = 0; i < cFile.fieldsCount; i++) {
		if ((cFile.fields[i].accessFlags & ACC_STATIC) != 0) {
			cList->staticFields[j].name = cFile.constantPool[cFile.fields[i].nameIndex].info.Utf8Info.bytes;
			cList->staticFields[j].descriptor = cFile.constantPool[cFile.fields[i].descriptorIndex].info.Utf8Info.bytes;
			cList->staticFields[j].fieldType.longType = 0;
			j++;
		}
	}
}

void appendClassList(ClassList** head, ClassFile classFile) {
    ClassList *cList, *cListAux;

    // Inicializa valores do novo elemento
    cList = malloc(sizeof(ClassList));
    cList->classFile = &classFile;
    cList->staticFieldsCount = 0;
    cList->staticFields = NULL;
    cList->objectsList = NULL;

    // Inicializa os campos estaticos
    initStaticFields(cList, classFile);

    // Atualiza ponteiro para proximo elemento
    if (*head == NULL) {
        *head = cList;
    } else {
        cListAux = *head;
        while(cListAux->nextClass != NULL)
            cListAux = cListAux->nextClass;
        cListAux->nextClass = cList;
    }
    cList->nextClass = NULL;
}

ClassFile* loadClass(Interpretador* interpretador, char* className) {
    ClassFile* cFile = getClassFileByName(interpretador->initClass, className);
    if (cFile == NULL) {
        cFile = readClass(className);
        appendClassList(&(interpretador->initClass), *cFile);
        // Inicia <clinit>
        if (getMethod(*cFile, "<clinit>", "()V") != NULL) {
            methodInit(className, "<clinit>", "()V", interpretador, 0, 0);
            methodExec(interpretador);
        }
    }
}

/*
void mainInit(char* className, Interpretador* interpretador, int paramsNumber, char** cmdLineParams) {
    char* methodName = "main", methodDescriptor = "([Ljava/lang/String;)V";
    ClassFile* cFile = loadClass(interpretador, className);
    pushFrame(&(interpretador->topStackFrame));
    frameInit(interpretador->initClass, *cFile, interpretador->topStackFrame, methodName, methodDescriptor);

}
*/
void methodInit(char* className, char* methodName, char* methodDescriptor, Interpretador* interpretador,int paramsNumber, int print) {
    int i;
    ClassFile* cFile = loadClass(interpretador, className);
    if (print)
        printClass(cFile, className);

    pushFrame(&(interpretador->topStackFrame));
    frameInit(interpretador->initClass, *cFile, interpretador->topStackFrame, methodName, methodDescriptor);
    for (i = countSlots(interpretador->topStackFrame->nextFrame->frame->topOperand, paramsNumber) - 1; i >= 0; i--) {
        if (interpretador->topStackFrame->nextFrame->frame->topOperand->operand.type32_64 == CAT2)
            i--;
        interpretador->topStackFrame->frame->localVarArray[i] = &(interpretador->topStackFrame->nextFrame->frame->topOperand);
    }
}
void methodExec(Interpretador* interpretador) {
    int ret_ = 0;
	u1 opcode;
	while (!ret_) {
		opcode = *(interpretador->topStackFrame->frame->execEnvir->pc);
        interpretador->topStackFrame->frame->execEnvir->pc++;
		ret_ = InstructionArray[opcode](interpretador);
	}
}
