#pragma once

/*
 * StrResLY.h
 *
 * Previously the interface to the string resource lex/yacc functions.
 * The lex/yacc parser (StrResL.cpp / StrResY.cpp) has been removed;
 * strings are now loaded from resources.pri via Windows App SDK ResourceManager
 * in stringsInitialise() (StarStrike/Text.cpp).
 *
 * This header is retained as an empty placeholder so that any translation
 * units that still include it do not fail to compile.
 */
