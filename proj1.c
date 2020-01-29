#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "proj1.h"

#define ESCAPE '\\'
#define ARGUMENT '#'
#define NEW_LINE '\n'
#define BRACE_OPEN '{'
#define BRACE_CLOSE '}'
#define COMMENT_START '%'

#define BRACE_OPEN_STR "{"
#define BRACE_CLOSE_STR "}"

#define NOT_FOUND 	-1
#define DEF			0
#define UNDEF		1
#define IFDEF		2
#define IF			3
#define INCLUDE		4
#define EXPANDAFTER	5

#define INIT_MACRO_CAPACITY 8
#define PROTECTED_MACROS 6
#define INIT_BUF 1024

typedef struct
{
	char *value;
	char *name;
} macro_t;

typedef struct
{
	macro_t **arr;
	int capacity;
	int index;
	int size;
} macrolist_t;

typedef struct
{
	char *charAt;
	int length;
} string_t;

typedef struct node
{
	char *data;
	struct node *next;
} node_t;

typedef struct
{
	node_t *head;
	int size;
} stack_t;

macro_t *createMacro(char *name, char *value);
macrolist_t *initMacros(void);
string_t *createString(char *str);
int findMacro(char *str, macrolist_t *macros);
int isValidArg(char *str);
int isValidDefArg(char *str);
int argIsAlnum(char *str);
node_t *createNode(char *data, node_t *next);
stack_t *createStack();
void push(stack_t *s, char *data);
void pushString(stack_t *s, char *str, int from, int commentStart, int commentEnd, int len);
char *pop(stack_t *s);
void flipStack(stack_t *s1, stack_t *s2);
int getStackTotalLength(stack_t *s);
string_t *stackToString(stack_t *s);
int isSpecialCharacter(char c);
long getFileLength(FILE *fp);
char *esc(char *str);
char *escAll(char *str);
char *removeBraces(char *str);
void def(macrolist_t *macros, char *name, char *value);
void undef(macrolist_t *macros, int index);
string_t *replace(char *original, char *value);
void processChunks(stack_t *s, macrolist_t *macros, stack_t *out);
void chunkString(string_t *str, stack_t *s);
string_t *readFile(char *filename);
string_t *readString(char *str);
string_t *destroyString(string_t *str);
stack_t *destroyStack(stack_t *s);
macro_t *destroyMacro(macro_t *macro);
macrolist_t *destroyMacros(macrolist_t *macros);

string_t *destroyString(string_t *str)
{
	if (!str)
		return NULL;
		
	free(str->charAt);
	free(str);

	return NULL;
}

stack_t *destroyStack(stack_t *stack)
{
	node_t *stackNode, *next;
	if (stack == NULL)
		return NULL;

	for (stackNode = stack->head; stackNode != NULL; stackNode = next)
	{
		next = stackNode->next;
		free(stackNode->data);
		free(stackNode);
	}
	free(stack);

	return NULL;
}

macro_t *destroyMacro(macro_t *macro)
{
	if (!macro)
		return NULL;
		
	free(macro->name);
	free(macro->value);

	free(macro);

	return NULL;
}

macrolist_t *destroyMacros(macrolist_t *macros)
{
	int i;

	if (!macros)
		return NULL;
		
	for (i = 0; i < macros->capacity; i++)
		destroyMacro(macros->arr[i]);
		
	free(macros->arr);
	free(macros);
		
	return NULL;
}

macro_t *createMacro(char *name, char *value)
{
	macro_t *macro = malloc(sizeof(macro_t));
	macro->name = name;
	macro->value = value;

	return macro;
}

macrolist_t *initMacros(void)
{
	macrolist_t *macros = malloc(sizeof(macrolist_t));
	macro_t **initialMacros	 = calloc(INIT_MACRO_CAPACITY, sizeof(macro_t));
	initialMacros[DEF]			= createMacro(strdup("def"), NULL);
	initialMacros[UNDEF]		= createMacro(strdup("undef"), NULL);
	initialMacros[IFDEF]		= createMacro(strdup("ifdef"), NULL);
	initialMacros[IF]			 = createMacro(strdup("if"), NULL);
	initialMacros[INCLUDE]		= createMacro(strdup("include"), NULL);
	initialMacros[EXPANDAFTER]	= createMacro(strdup("expandafter"), NULL); 
		
	macros->arr = initialMacros;
	macros->size = macros->index = 6;
	macros->capacity = INIT_MACRO_CAPACITY;

	return macros;
}

string_t *createString(char *str)
{
	string_t *newStr = calloc(1, sizeof(string_t));
	if (!str || !newStr)
		return NULL;

	newStr->length = strlen(str);
	newStr->charAt = strdup(str);

	return newStr;
}

int findMacro(char *str, macrolist_t *macros)
{
	int len, index = -1, i, start, end;
	macro_t *macro;
	char *tmp;

	if (!str || !macros || !(len = strlen(str)))
		return -1;
		
	tmp = calloc(len + 1, sizeof(char));

	start = str[0] == BRACE_OPEN || str[0] == ESCAPE ? 1 : 0;
	end = str[len - 1] == BRACE_CLOSE && str[0] != ESCAPE ? len - 1 : len;

	for (i = 0; i < end - start; i++)
		tmp[i] = str[i + start];

	for (i = 0; i < macros->capacity && index == -1; i++)
	{
		macro = macros->arr[i];
		if (macro)
		{
			if (macros->arr[i] && !strcmp(tmp, macros->arr[i]->name))
				index = i;
		}
	}

	free(tmp);

	return index;
}

int isValidDefArg(char *str)
{ 
	return str && strlen(str) > 2 && isValidArg(str); 
}

int isValidArg(char *str)
{
	int len, braces, i;
	if (!str || str[0] != BRACE_OPEN || (len = strlen(str)) < 2 || str[len - 1] != BRACE_CLOSE)
		return 0;

	braces = 0;
	len--;

	for (i = 1; i < len; i++)
	{
		if (str[i] == BRACE_OPEN && str[i - 1] != ESCAPE)
			braces++;
		else if (str[i] == BRACE_CLOSE && str[i - 1] != ESCAPE)
			braces--;
		
		if (braces < 0)
			return 0;
	}
		
	return braces == 0;
}

int argIsAlnum(char *str)
{
	int len = strlen(str) - 1, i;

	for (i = 1; i < len; i++)
		if (!isalnum(str[i]))
			return 0;
		
	return 1;
}

node_t *createNode(char *data, node_t *next)
{
	node_t *node;
	char *str;
	int i, len = strlen(data);

	if (!(node = malloc(sizeof(node_t))) ||
		!(str = calloc(len + 1, sizeof(char))))
		DIE("%s", "Bad memory createNode");

	for (i = 0; i < len; i++)
		str[i] = data[i];

	node->data = str;
	node->next = next;

	return node;
}

stack_t *createStack()
{
	return calloc(1, sizeof(stack_t));
}

void push(stack_t *s, char *data)
{
	if (!s || !data || !data[0])
		return;
		
	node_t *node;

	if (node = createNode(data, s->head))
	{
		s->head = node;
		s->size++;
	}
	else
	{
		DIE("%s", "Bad memory push");
	}
}


void pushString(stack_t *s, char *str, int from, int commentStart, int commentEnd, int len)
{
	if (!s || !str || from < 0)
		return;

	char *data = calloc(len + 1, sizeof(char));
	int i, j;

	if (commentStart != commentEnd)
	{
		for (i = 0; i + from < commentStart && str[i + from]; i++)
		{
			data[i] = str[i + from];
			len--;
		}

		for (j = 0; len-- && str[j + commentEnd]; j++)
			data[i++] = str[j + commentEnd];

	}
	else for (i = from; i < from + len && str[from]; i++)
		data[i - from] = str[i];

	push(s, data);
	free(data);
}

char *pop(stack_t *s)
{
	node_t *node;
	char *popped;

	if (!s || !(node = s->head))
		return NULL;

	popped = strdup(node->data);
	free(node->data);
	s->head = node->next;
	s->size--;
	free(node);

	return popped;
}

void flipStack(stack_t *s1, stack_t *s2)
{
	char *str;
	while (s1->head)
	{
		str = pop(s1);
		push(s2, str);
		free(str);
	}
}

int getStackTotalLength(stack_t *s)
{
	int len = 0;
	for (node_t *tmp = s->head; tmp; tmp = tmp->next)
		len += strlen(tmp->data);
		
	return len;
}

string_t *stackToString(stack_t *s)
{
	string_t *str;
	char *tempString;
	node_t *tmp;
	int i = 0, j;

	if (!s || !s->size)
		return NULL;
		
	str = malloc(sizeof(string_t));
	str->length = getStackTotalLength(s);
	str->charAt = calloc(str->length + 1, sizeof(char));

	for (tmp = s->head; tmp; tmp = tmp->next)
	{
		tempString = tmp->data;
		
		for (j = 0; tempString[j]; j++)
			str->charAt[i++] = tempString[j];

	}

	return str;
}

int isSpecialCharacter(char c)
{
	switch(c)
	{
		case ESCAPE:
		case ARGUMENT:
		case BRACE_OPEN:
		case BRACE_CLOSE:
		case COMMENT_START:
			return 1;
		default: return 0;	
	}
}

int isPreservedCharacter(char c)
{
	switch(c)
	{
		case '[':
		case ']':
		case '(':
		case ')':
		case '+':
		case '-':
		case '*':
		case '/':
		case '=':
			return 1;
		default: return 0;	
	}
}

long getFileLength(FILE *fp)
{
	int res;

	// Reach EOF
	fseek(fp, 0, SEEK_END);

	// Save position indicator
	res = ftell(fp);
		
	// Reset position indicator
	fseek(fp, 0, SEEK_SET);

	return res;
}

char *esc(char *str)
{
	char *escapedStr, *tmp;
	int len = strlen(str);
	int i, j, last;

	tmp = calloc(len + 1, sizeof(char));
	for (i = j = 0; i < len; i++)
	{
		if (str[i] == ESCAPE && i + 1 < len &&
			!isSpecialCharacter(str[i + 1]) &&
			!isPreservedCharacter(str[i + 1]))
			tmp[j++] = str[(last = ++i)];
		else 
			tmp[j++] = str[i];
	}

	// Save memory..
	escapedStr = calloc(j + 1, sizeof(char));
	for (i = 0; i < j; i++)
		escapedStr[i] = tmp[i];

	free(tmp);
	return escapedStr;
}

char *escAll(char *str)
{
	char *escapedStr, *tmp;
	int len = strlen(str);
	int i, j, last;

	tmp = calloc(len + 1, sizeof(char));
	for (i = j = 0; i < len; i++)
	{
		if (str[i] == ESCAPE && i + 1 < len &&
			!isPreservedCharacter(str[i + 1]))
			tmp[j++] = str[(last = ++i)];
		else 
			tmp[j++] = str[i];
	}

	// Save memory..
	escapedStr = calloc(j + 1, sizeof(char));
	for (i = 0; i < j; i++)
		escapedStr[i] = tmp[i];

	free(tmp);
	return escapedStr;
}

char *removeBraces(char *str)
{
	char *newStr;
	int len, i, j;

	if (!str || !(len = strlen(str)))
		return NULL;

	newStr = calloc(len + 1, sizeof(char));

	i = (str[0] == BRACE_OPEN);
	for (j = 0; i < len - 1; i++)
		newStr[j++] = str[i];

	if (str[i] != BRACE_CLOSE)
		newStr[j] = str[i];

	return newStr;
}

void def(macrolist_t *macros, char *name, char *value)
{
	macro_t *newMacro;
	macro_t **newArr;
	int i, j;

	// Expand (double size)
	if (macros->index + 1 == macros->capacity || macros->size == macros->capacity)
	{
		newArr = calloc(macros->capacity * 2, sizeof(macro_t *));

		// Get rid of holes
		for (i = j = 0; i < macros->capacity; i++)
			if (macros->arr[i])
				newArr[j++] = macros->arr[i];
		
		// Cleanup
		free(macros->arr);

		macros->capacity = macros->capacity * 2;
		macros->arr = newArr;
		macros->index = j;
	}

	macros->arr[macros->index++] = createMacro(removeBraces(name), removeBraces(value));
	macros->size++;
}

void undef(macrolist_t *macros, int index)
{
	if (!macros || index >= macros->capacity)
		return;
		
	free(macros->arr[index]->name);
	free(macros->arr[index]->value);
	free(macros->arr[index]);
	macros->arr[index] = NULL;
	macros->size--;
}

string_t *replace(char *original, char *value)
{
	string_t *newStr = calloc(1, sizeof(string_t));
	int valLen = strlen(value), i, j;
	int originLen = strlen(original);
	int totalLen = 0;
		
	for (i = totalLen = 0; i < originLen; i++)
	{
		if (original[i] == ESCAPE)
		{
			if (isSpecialCharacter(original[i + 1])) 
			{
				i++;
				continue;
			}
		}
		else if (original[i] == ARGUMENT)
			totalLen += valLen - 1;
	}

	newStr->length = totalLen + i + 1;
	newStr->charAt = calloc(newStr->length + 1, sizeof(char));

	for (i = j = 0; i < originLen; i++)
	{
		if (original[i] == ESCAPE)
		{
			newStr->charAt[j++] = original[i++];

			if (i < originLen)
				newStr->charAt[j++] = original[i];
		}
		else if (original[i] == ARGUMENT)
		{
			strcat(newStr->charAt + j, value);
			j += valLen;
		}
		else newStr->charAt[j++] = original[i];
	}

	return newStr;
}

void processChunks(stack_t *s, macrolist_t *macros, stack_t *out)
{
	int macroId, len, i;
	char *filename, *temp1, *temp2;
	string_t *arg1, *arg2, *before, *after;
	stack_t *beforeStack, *afterStack, *beforeOut, *afterOut;

	while (s->head)
	{
		// TODO: check if head->data is null

		macro_t *macro;
		for (i = 0; i < macros->size; i++)
		{
			macro = macros->arr[i];
			if (macro)
			{
				temp1 = macro->name;
				temp2 = macro->value;
			}

			if (i >= PROTECTED_MACROS)
			{
				if (macro)
				{
					temp1 = macro->name;
					temp2 = macro->value;
				}
			}
		}

		if (strlen(s->head->data) == 1)
		{
			if (s->head->data[0] == ESCAPE && s->head->next && isSpecialCharacter(s->head->next->data[0]))
			{
				len = 2 + strlen(s->head->next->data);
				filename = calloc(len, sizeof(char));

				filename[0] = ESCAPE;
				for (i = 1; i < len - 1; i++)
					filename[i] = s->head->next->data[i - 1];

				free(pop(s));
				free(pop(s));

				temp1 = esc(filename); // TODO: fix
				push(s, filename); // TODO: Escape
				free(filename);
				free(temp1);
			}

			temp1 = pop(s);
			push(out, temp1);
			free(temp1);

			continue;
		}

		// (Else)
		switch (s->head->data[0])
		{
			case ESCAPE:
				if (isSpecialCharacter(s->head->data[1]) || isPreservedCharacter(s->head->data[1]))
				{
					temp1 = pop(s);
					temp2 = esc(temp1); // TODO: fix
					push(out, temp2);
					free(temp1);
					free(temp2);
				}
				else
				{
					macroId = findMacro(s->head->data, macros);
					switch (macroId)
					{
						case NOT_FOUND:
							DIE("%s", "Invalid macro");
							// TODO: Terminate program
							break;

						case DEF:
							if (!s->head->next || !s->head->next->next)
							{
								DIE("%s", "Missing argument(s) for def");
								// TODO: Terminate program
							}

							macroId = findMacro(s->head->next->data, macros);

							if (macroId != NOT_FOUND)
							{
								DIE("%s", "Macro already defined");
								// TODO: Terminate program
							}

							if (!isValidDefArg(s->head->next->data) ||
								!isValidArg(s->head->next->next->data))
							{
								DIE("%s", "Bad argument(s) for def");
								// TODO: Terminate program
							}

							if (!argIsAlnum(s->head->next->data))
							{
								DIE("%s", "New defenition requires alpha-numberic chars only");
								// TODO: Terminate program
							}

							def(macros, s->head->next->data, s->head->next->next->data);

							free(pop(s));
							free(pop(s));
							free(pop(s));

							break;

						case UNDEF:
							if (!s->head->next)
							{
								DIE("%s", "Missing argument(s) for def");
								// TODO: Terminate program
							}

							macroId = findMacro(s->head->next->data, macros);

							if (macroId == NOT_FOUND)
							{
								DIE("%s", "Macro is not defined (can't undef)");
								// TODO: Terminate program
							}

							if (macroId < PROTECTED_MACROS)
							{
								DIE("%s", "Can't undef protected macros");
								// TODO: Terminate program
							}

							undef(macros, macroId); // TODO: CHECK IF FREES MEMORY

							free(pop(s));
							free(pop(s));

							break;

						case IFDEF:
							if (!s->head->next || !s->head->next->next ||
								!s->head->next->next->next)
							{
								DIE("%s", "Missing argument(s) for if ifdef");
								// TODO: Terminate program
							}

							if (!isValidArg(s->head->next->data) ||
								!isValidArg(s->head->next->next->data) || 
								!isValidArg(s->head->next->next->next->data))
							{
								DIE("%s", "Bad argument(s) for ifdef");
								// TODO: Terminate program
							}

							if (findMacro(s->head->next->data, macros) == NOT_FOUND)
								temp1 = removeBraces(s->head->next->next->next->data);
							else temp1 = removeBraces(s->head->next->next->data);

							arg1 = readString(temp1);
							free(temp1);

							// ifdef (DEF) (THEN) (ELSE)
							free(pop(s));	 // ifdef
							free(pop(s));	 // (DEF)
							free(pop(s));	 // (THEN)
							free(pop(s));	 // (ELSE)

							chunkString(arg1, s);
							destroyString(arg1);

							break;

						case IF:
							if (!s->head->next || !s->head->next->next ||
								!s->head->next->next->next)
							{
								DIE("%s", "Missing argument(s) for if");
								// TODO: Terminate program
							}

							if (!isValidArg(s->head->next->data) ||
								!isValidArg(s->head->next->next->data) || 
								!isValidArg(s->head->next->next->next->data))
							{
								DIE("%s", "Bad argument(s) for if");
								// TODO: Terminate program
							}

							if (strlen(s->head->next->data) < 3)
								temp1 = removeBraces(s->head->next->next->next->data);
							else temp1 = removeBraces(s->head->next->next->data);

							arg1 = readString(temp1);
							free(temp1);

							// TODO: popn(s, 4);
							free(pop(s));
							free(pop(s));
							free(pop(s));
							free(pop(s));
							
							chunkString(arg1, s);
							destroyString(arg1);
							break;

						case INCLUDE:
							if (!s->head->next)
							{
								DIE("%s", "Missing argument(s) for if include");
								// TODO: Terminate program
							}
							
							if (!isValidArg(s->head->next->data))
							{
								DIE("%s", "Bad argument(s) for include");
								// TODO: Terminate program
							}

							filename = removeBraces(s->head->next->data);
							
							free(pop(s));
							free(pop(s));
							
							arg1 = readFile(filename);
							free(filename);
							chunkString(arg1, s);
							destroyString(arg1);
							break;

						case EXPANDAFTER:
							if (!s->head->next || !s->head->next->next)
							{
								DIE("%s", "Missing argument(s) for expandafter");
								// TODO: Terminate program
							}

							if (!isValidArg(s->head->next->data) ||
								!isValidArg(s->head->next->next->data))
							{
								DIE("%s", "Bad argument(s) for expandafter");
								// TODO: Terminate program
							}

							free(pop(s));

							// After
							temp1 = pop(s);
							temp2 = removeBraces(temp1);
							free(temp1);
							after = readString(temp2);
							free(temp2);

							// Before
							temp1 = pop(s);
							temp2 = removeBraces(temp1);
							free(temp1);
							before = readString(temp2);
							free(temp2);

							beforeStack = createStack();
							beforeOut = createStack();

							chunkString(before, beforeStack);
							processChunks(beforeStack, macros, beforeOut); // FIXME: check this

							destroyStack(beforeStack);
							beforeStack = createStack();
							flipStack(beforeOut, beforeStack);

							// TODO: destroy before
							destroyString(before);
							before = stackToString(beforeStack);

							// Concat strings
							if (before)
							{
								arg1 = calloc(1, sizeof(string_t));
								arg1->length = before->length + after->length;
								arg1->charAt = calloc(arg1->length + 1, sizeof(char));
								
								for (i = 0; i < after->length; i++)
									arg1->charAt[i] = after->charAt[i];
								
								for (len = 0; len < before->length; len++)
									arg1->charAt[i++] = before->charAt[len];
							}
							else arg1 = createString(after->charAt);

							chunkString(arg1, s);
							
							// cleanup
							destroyString(before);
							destroyString(after);
							destroyString(arg1);
							destroyStack(beforeStack);
							destroyStack(beforeOut);
							break;
						default:
							if (!s->head->next)
							{
								DIE("%s", "Missing argument(s) for custom macro");
								// TODO: Terminate program
							}
							if (!isValidArg(s->head->next->data))
							{
								DIE("%s", "Bad argument(s) for custom macro");
								// TODO: Terminate program
							}

							temp1 = removeBraces(s->head->next->data);

							arg1 = replace(macros->arr[macroId]->value, temp1);
							free(temp1);
							free(pop(s));
							free(pop(s));
							chunkString(arg1, s);
							destroyString(arg1);
							break;
					}
				}
				break;
			
			case BRACE_OPEN:
				temp1 = pop(s);
				temp2 = removeBraces(temp1);
				free(temp1);

				arg1 = createString(temp2);
				free(temp2);

				push(s, BRACE_CLOSE_STR);
				chunkString(arg1, s);
				push(s, BRACE_OPEN_STR);

				destroyString(arg1);
				break;

			default:
				temp1 = pop(s);
				temp2 = esc(temp1); // TODO FIX

				push(out, temp1);

				free(temp1);
				free(temp2);
				break;
		}
	}
}

void chunkString(string_t *str, stack_t *s)
{
	// Capture from i to end
	// when reaching %, brace, escape
	int braces, chunkLen, i, commentLen, commentStart;
	stack_t *tmpStack = createStack();

	// NOTE: i - commentLen - chunkLen

	commentLen = braces = chunkLen = commentStart = 0;
	for (i = 0; i <= str->length; i++)
	{
		switch (str->charAt[i])
		{
			case COMMENT_START:
				// End chunk
				if (chunkLen && !commentLen && !braces)
				{
					// TODO: Push chunk to stack
					pushString(tmpStack, str->charAt, i - chunkLen, 0, 0, chunkLen);
					
					// TODO: reset chunkLen
					chunkLen = 0;
				}

				// Init
				commentLen = 0;
				do
				{
					// Count '%' (COMMENT_START)
					commentLen++;
					
					// Discard everything on this line
					for (; ++i <= str->length && str->charAt[i] != NEW_LINE; commentLen++)
						;

					// Discard whitespace on next line
					for (commentLen++; ++i <= str->length && isspace(str->charAt[i]); commentLen++)
						;

					// Preserve first non-whitespace character
					--commentLen;
				} while (i <= str->length && str->charAt[i--] == COMMENT_START);

				commentStart = i - commentLen++;

				
				if (!braces)
					commentLen = 0;
				// else commentLen++;
				break;
			
			case BRACE_OPEN:
				// There are no braces and there is a chunk
				if (!braces && chunkLen)
				{
					// TODO: Push chunk to stack
					pushString(tmpStack, str->charAt, i - chunkLen - commentLen,
						commentStart, commentStart + commentLen, chunkLen);
					
					// TODO: reset chunkLen
					chunkLen = commentLen = commentStart = 0;
				}

				// Include in chunk
				chunkLen++;

				// Increment braces count
				braces++;
				break;

			case BRACE_CLOSE:
				// Include in chunk
				chunkLen++;
				
				// Close brace and check if chunk is closed too
				if (!--braces)
				{
					// TODO: Push chunk to stack
					pushString(tmpStack, str->charAt, i - chunkLen - commentLen + 1,
						commentStart, commentStart + commentLen, chunkLen);
					
					// TODO: reset chunkLen
					chunkLen = commentLen = commentStart = 0;
				}
				break;

			case NEW_LINE:
				if (braces)
					chunkLen++; // Include in chunk
				else
				{
					if (chunkLen)
					{
						// TODO: Push chunk to stack
						pushString(tmpStack, str->charAt, i - chunkLen - commentLen,
							commentStart, commentStart + commentLen, chunkLen);
						
						// TODO: reset chunkLen
						chunkLen = commentLen = commentStart = 0;
					}
					push(tmpStack, "\n");
				}
				break;
			
			case ESCAPE:
				// TODO:
				// Check if has next character
				if (i + 1 > str->length)
				{
					// TODO: invalid syntax
					// Break out of loop..
				}

				// TODO:
				// If next character is special character (%, {, }, \, #)
				if (isSpecialCharacter(str->charAt[i + 1]))
				{
					i++;			// Skip processing next character
					chunkLen++;	 // Include in chunk
				}

				else if (chunkLen && !braces)
				{
					// TODO: Push chunk to stack
					pushString(tmpStack, str->charAt, i - chunkLen - commentLen,
						commentStart, commentStart + commentLen, chunkLen);
					
					// TODO: reset chunkLen
					chunkLen = commentLen = commentStart = 0;
				}
				
				// Include in chunk
				chunkLen++;
				break;
				
			default:
				// Include in chunk
				chunkLen++;
				break;
		}
		// TODO: verify BRACES is not negative
	}

	if (chunkLen) // TODO: And validate braces/invalidc syntax for escp. char
	{
		pushString(tmpStack, str->charAt, i - chunkLen - commentLen,
			commentStart, commentStart + commentLen, chunkLen);
	}

	flipStack(tmpStack, s);
	destroyStack(tmpStack);
}

string_t *readFile(char *filename)
{
	FILE *fp;
	string_t *str = calloc(1, sizeof(string_t));
	int fLen;
		
	if (!(fp = fopen(filename, "r")))
		DIE("%s", "Invalid file");

	// Get the length of the file
	fLen = getFileLength(fp);

	// Allocate memory to save file into memory
	str->length = fLen;
	str->charAt = calloc(str->length + 1, sizeof(char));

	// Read contents of file into str
	fread(str->charAt, sizeof(char), fLen, fp);

	return str;
}

string_t *readString(char *str)
{
	return createString(str);
}

string_t *readStdin()
{
	int i, j, len;
	char c, *tmp;
	string_t *str = calloc(1, sizeof(string_t));
	str->length = INIT_BUF;
	str->charAt = calloc(str->length + 1, sizeof(char));
		
	// Read contents of file into str
	for (i = 0; (c = fgetc(stdin)) != EOF; i++)
	{
		if (i == str->length)
		{
			str->length *= 2;
			tmp = calloc(str->length + 1, sizeof(char));

			for (j = 0; j < i; j++)
				tmp[j] = str->charAt[j];

			free(str->charAt);
			str->charAt = tmp;
		}

		str->charAt[i] = c;
	}

	len = strlen(str->charAt);
	str->length = len;
	tmp = calloc(len + 1, sizeof(char));

	for (j = 0; j < len; j++)
		tmp[j] = str->charAt[j];

	free(str->charAt);
	str->charAt = tmp;

	return str;
}

string_t *readFiles(string_t *str, char *filename)
{
	FILE *fp;
	int fLen, i, j = str->length;
	char *tmp = str->charAt;

	if (!(fp = fopen(filename, "r")))
		return str; // TODO: Error opening file (filename)

	// Get the length of the file
	fLen = getFileLength(fp);

	// Allocate memory to save file into memory
	str->length += fLen;
	str->charAt = calloc(str->length + 1, sizeof(char));

	for (i = 0; i < j; i++)
		str->charAt[i] = tmp[i];
	free(tmp);

	// Read contents of file into str
	fread(str->charAt + j, sizeof(char), fLen, fp);

	return str;
}

int main(int argc, char *argv[])
{
	int i;
	char *tmp, *tmp1;
	string_t *str;
	macrolist_t *macros = initMacros();
	stack_t *stack = createStack();
	stack_t *out = createStack();
	stack_t *finalOutput = createStack();

	// Read from stdin
	if (argc == 1)
	{
		str = readStdin();
	}
	else
	{
		str = readFile(argv[1]);
		for (i = 2; i < argc; i++)
			str = readFiles(str, argv[i]);
	}

	chunkString(str, stack);
	processChunks(stack, macros, out);

	flipStack(out, finalOutput);

	while (finalOutput->head)
	{
		tmp = pop(finalOutput);
			tmp1 = escAll(tmp);
		fprintf(stdout, "%s", tmp1);
		free(tmp);
		free(tmp1);
		fflush(stdout);
	}

	destroyStack(out);
	destroyStack(stack);
	destroyStack(finalOutput);
	destroyString(str);
	destroyMacros(macros);

	return 0;
}