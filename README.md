An Almost Complete Jinja2 Port to C
===========================
# Currently it is still under heavy testing.

# Introduction
This is a Jinja2 template engine porting to C. It contains *NEARLY* all the Jinja2 features
but doesn't require a full python as a backend to make it work. It also adds some useful
extension Jinja2.

# Feature
1. Nearly all Jinja2 sytanx is directly supported, all the Jinja2 semantic can be expressed.
2. Implement in pure C without any dependency except libc.
3. Around 10000 lines of code and you could render common jinja2 template, compared to embedding CPython( aronud 1 million lines of code ) plus Jinja2 library.
4. Designed for embeding as a library. Using C API, user is able to register different objects/value used at runtime.
5. Multiple extension to Jinja2 template, including multiple inheritance, multiple level inheritance and move value to outer scope.
6. Builtin optimizer that will do constant folding and simple jump rewriting.
7. Specialized GC strategy, allow move value from inner scope to outer scope. Also it is efficient since no mark based GC algorithm is used.
8. Customize template rendering through simple Json file.

# Comparison
TODO
