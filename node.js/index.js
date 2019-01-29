var morph = require('./build/Release/morph');

function init(dir) {
	return morph.init(dir);
}

function destroy() {
	return morph.destroy();
}

function str_intersect_str(str1, str2) {
	return morph.str_intersect_str(str1, str2);
}

function str_intersect_str2(str1, str2) {
	return morph.str_intersect_str2(str1, str2);
}

function str_case_str(str1, str2) {
	return morph.str_case_str(str1, str2);
}

function normalize_phrase(str) {
	return morph.normalize_phrase(str);
}

exports.init               = init;
exports.destroy            = destroy;
exports.str_intersect_str  = str_intersect_str;
exports.str_intersect_str2 = str_intersect_str2;
exports.str_case_str       = str_case_str;
exports.normalize_phrase   = normalize_phrase;
