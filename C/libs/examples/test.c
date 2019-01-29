#include <morph/morph.h>

#define TEXT "россии президент путин "
#define TEXT1 "россии президенту путину "

#define TEXT3 "Россия "
#define TEXT4 "России "

#define TEXT5 "палка, палкой огуречик вот и вышел человечек"

int
main(int argc, char **argv)
{
    double pr = 0.0;
    
    morph_t *morph = morph_new(MORPH_PATH_DICTS);
	if (morph == NULL) {
		fprintf(stderr, "Allocated failed.\n");
        return -1;
	}
    
    morph_doc_t *doc    = morph_doc_new(morph, TEXT1, strlen(TEXT1), 1);
	if (doc == NULL) {
		fprintf(stderr, "Allocated failed.\n");
        return -1;
	}
	
    morph_doc_t *search = morph_doc_new(morph, TEXT, strlen(TEXT),   0);
	if (search == NULL) {
		fprintf(stderr, "Allocated failed.\n");
        return -1;
	}
	
    pr = morph_doc_intersect_doc(doc, search);
    printf("pr = %lf\n", pr);
    
    pr = morph_str_intersect_str(morph, TEXT1, TEXT);
    printf("pr = %lf\n", pr);

    pr = morph_str_intersect_str(morph, TEXT4, TEXT3);
    printf("pr = %lf\n", pr);

	pr = morph_str_intersect_str(morph, "палка, палка огуречик вот и вышел человечек", "точка, точка запятая - вышла рожица кривая");
    printf("pr = %lf\n", pr);

	puts(morph_normalize_form(TEXT5, morph, strlen(TEXT5)));
	
    morph_doc_delete(doc);
    morph_doc_delete(search);
    
    morph_delete(morph);
}