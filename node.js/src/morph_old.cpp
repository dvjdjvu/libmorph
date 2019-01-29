#include <v8.h>
#include <node.h>

#include <iostream>

extern "C" {
    #include <morph/morph.h>
}

using namespace node;
using namespace v8;
using namespace std;

static morph_t *morph = NULL;

static Handle<Value> init(const Arguments& args)
{   
    //for (int i = 0; i < args.Length(); i++) {
    //    String::Utf8Value str(args[i]);
    //}

    if (morph != NULL) {
        morph_delete(morph);
        morph = NULL;
    }

    if (args.Length() == 0) {
        morph = morph_new("/local/morph/dicts");
    } else {
        String::Utf8Value str(args[0]);
        printf("%s\n", *str);
        morph = morph_new(*str);
    }

    if (morph == NULL) {
        return Number::New(0);
    }
    
    return Number::New(1);
}

static Handle<Value> destroy(const Arguments& args)
{   
    morph_delete(morph);
    morph = NULL;
}

static Handle<Value> normalize_phrase(const Arguments& args)
{   
    char *normalize_morph_form = NULL;
    
    //cout << "size " << args.Length() << endl;
    
    if (args.Length() == 0) {
        return String::New("");
    }
    
    String::Utf8Value query(args[0]);
    
    normalize_morph_form = morph_normalize_form(*query, morph->multi_morphology, query.length());
    if (normalize_morph_form == NULL) {
        return String::New("");
    }
    
    Handle<Value> norm = String::New(normalize_morph_form);

    free(normalize_morph_form);

    return norm;
}

static Handle<Value> str_intersect_str(const Arguments& args)
{
    if (args.Length() < 2) {
        return Number::New(0.0);
    }
    
    String::Utf8Value doc_s(args[0]);
    String::Utf8Value search_s(args[1]);
    
    return Number::New(morph_str_intersect_str(morph, *doc_s, *search_s));
    
}

static Handle<Value> str_intersect_str2(const Arguments& args)
{
    if (args.Length() < 2) {
        return Number::New(0.0);
    }
    
    String::Utf8Value doc_s(args[0]);
    String::Utf8Value search_s(args[1]);
    
    return Number::New(morph_str_intersect_str2(morph, *doc_s, *search_s));
    
}

static Handle<Value> str_case_str(const Arguments& args)
{
    if (args.Length() < 2) {
        return Number::New(0.0);
    }
    
    String::Utf8Value doc_s(args[0]);
    String::Utf8Value search_s(args[1]);
    
    return Number::New(morph_str_case_str(morph, *doc_s, *search_s));
    
}

extern "C" void init(Handle<Object> target)
{
    target->Set(String::New("version"), String::New("0.0.1"));

    NODE_SET_METHOD(target, "init",    init);
    NODE_SET_METHOD(target, "destroy", destroy);
    NODE_SET_METHOD(target, "str_intersect_str",  str_intersect_str);
    NODE_SET_METHOD(target, "str_intersect_str2", str_intersect_str2);
    NODE_SET_METHOD(target, "str_case_str",       str_case_str);
    NODE_SET_METHOD(target, "normalize_phrase",   normalize_phrase);
}
