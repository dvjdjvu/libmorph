#ifndef __TEXTPROCESSOR_TOKENIZER_H_
#define __TEXTPROCESSOR_TOKENIZER_H_

#include <wchar.h>
#include <stdlib.h>

ssize_t tokenize(const char *text, const char **token_start, const char **token_end, const wchar_t **wide_token, void **memo);
void final_tokenize(void *memo);

#endif /* __TEXTPROCESSOR_TOKENIZER_H_ */
