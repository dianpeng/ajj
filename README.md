An Almost Complete Jinja2 Port to C
===========================
# This Library Is Still Under Heavy Testing

# Features.
1. Support nearly all jinja2 syntax and related python feature.
2. Support flexible extension. User could register new type, object and other values.
3. Multiple extension to Jinja2 , including multi-inheritance , multi-level inheritance , move value to outer scope and return statement.
4. Automatic garbage collection through lexical scope.
5. Dynamic global variable value binding through Json file or template itself.
6. Fully UTF encoding support.
7. Small code base with less 12000 lines of C code. Design for embedding.
8. Default value for macro parameter _must_ be constant value or evaluated to constant.
9. Key value pair style function call is not supported.

# Not Supported Jinja2 Features
1. Recurisve loop is not supported, related field in Loop object is not supported as well.
2. Tuple is not supported, tuple syntax is supported but it is automatically convert to list.
```
{% set MyTuple = (1,2,3) %}
```
is esentially same as
```
{% set MyTuple = [1,2,3] %}
```.

3. One argument test invoke without parenthesis is not supported.So you cannot write
```
{% if 2 is dividable 3 %}
```
but need to write as
```
{% if 2 is dividable(3) %}
```.
4. Whitespace control is not optional. The default whitespace control is applied. But the syntax is supported but takes no effect.
```
 {% do SomeThinig %}
```
is same as
```
 {% do SomeThing -%}
```.

5. Inlude option is not support, but we allow extension for context definition when doing inclusion.

6. Line comments is not supported.

7. Convert string is not achieved by ~ , but using + sign. So we don't have a ~ sign.

8. Macro object is not supported inside of macro block. But we have list of builtins allow you get related information. Not support macor is basically because it is a redundancy .

9. Scopped block is supported through different syntax but not by tag a "scoped" keyword. To use variable in outer scope in nested block scope, the syntax is as follow ,

```
{% set outer_var = [] %}
  {% block MyBlock(outer_var) %}
    {{ outer_var }}
  {% endblock %}
```.

# Extension to Jinja2
1. Context definition through json file or template variable. When you want to include another template, you could customize the rendering behavior by setting upvalue for this template. Upvalue is the context variable that cannot be resolved inside of the
template but need to provided externally.

```
    {% include 'MyFile.html' %}
      {% GlobalVariable1 = "SomeValue" %}
      {% GlobalVariable2 = "SomeValue2" %}
    {% endinclude %}
```

The code above shows customize the MyFile.html rendering by setting different global variables. Also, you could provide a json
file to define those variables externally. Then using syntax

```
{% include 'MyFile.html' json 'MyJsonFile.json' %}
  {% GlobalVariable1 = "SomeValue" optional %}
  {% GlobalVariable2 = "SomeValue2" overwrite %}
{% endinclude %}
```

The above code allow us to import a json file to define global variable. Also you are allowed to define global variable inside of
the template. User may notice that optional modifier may tag to those entry ,which allow them to overwrite the value inside of json file or provides as a optional one if corresponding entry is not shown up in json file.


2.We support multi-inheritance.

```
  {% extends 'Template1' %}
  {% extends 'Template2' %}
```
Each extended template will be evaluated separatly and accordingly.

3. Ajj also support multiple level inheritance. Each template you extends can have its own extension as well.

4. Move Value to outer scope. In jinja2, we have no way to let the variable in outer scope to access value in inner scope because the scope rules. User may need to bypass it using array + do scope. Now in AJJ, a special block is provided to do so.

```
  {% set OuterVar = None %}
  {% with InnerVar = [] %}
  {% move OuterVar = InnerVar %}
  {% endwith %}
```
The above code will move the value of InnerVar to OuterVar.

5. Return statements
Although questionable, but having a return statements may be useful for some tasks. Using return statements your macro could be a real function that perform certain tasks.

```
{% macro SumOfArray( arr ) %}
  {% set result = 0 %}
  {% for x in arr %}
    {% set cur = result + x %}
    {% move result = cur %}
  {% endfor %}
  {% return result %}
{% endmacro %}

{{ SumOfArray([1,2,3,4,5]) }}
```

This code implement a macro that calculates the sum of an array and it should output 15.

# How it works ?
The Ajj library implements a complete script infrastructure. For jinja2 , since it compiles to python code , it could reuse all the python backbone to support its execution. However we don't want to reuse the existed cpython implementation because it is too big. With library, the cpython has nearly 1 million lines of code which is not suitable for embeding. Therefore, from Ajj point of view, it needs to recreate , not only jinja2 , but all the python part as well. However by carefully researching jinja2 grammar requirements, we can optionally drop some feature python needs but not needed inside of jinja2.

Ajj implements a one pass parser which directly generate bytecode sequences without AST tree. The major reason is this style requires less memory . Also we don't need AST to do analysing or exported as a library, directly go for bytecode will be faster. A peephole optimization phase will be applied on generated bytecode sequence later on. It will do constant folding and jump rewriting , also it will remove dummy instruction generated during the parsing phase. Lastly, the generated bytecode will be executed through our VM.Also we implement a lexical scope based GC, which is really simple but enough for Jinja2 grammar. The reason to not perfre a mark based algorithm is because it is too heavy weight for Jinja2 and also not required at all because Jinja2 has very stict lexical scope rules. A reference conut can not be applied without extra mark phase simply because it can leak memory for reference cycle. Read more in GC section.

# GC
Garbage collection is extreamly simple. During parsing phase we generate instruction ENTER and EXIT when we enter a lexical scope and exit lexical scope. All the memory allocated inside of a specific lexical scope will goes to its corresponding memory scope, we call GC scope. GC scope are chained together as VM exedcute ENTER. So each ENTER instrcution result in a new GC scope created and chain to its parent lexical scope's GC scope. Once we exit the lexical scope we could just walk the memory owned by this scope and delete them. However we do have some cases the memory gonna "escape". This is totally fine, because each heap based object will have a GC scope pointer inside of it, everytime we do an assignment, we just need to check whether the target GC scope is outer more than the source value's GC scope. Because each GC scope also assign a number to indicate its lexical scope position, we can simply compare this integer to decide whether we need to move the value from the source scope to target GC scope. The smaller the value is, the longer the scope's life cycle is. The root of GC scope is the global GC scope which will not be deleted until we delete the whole Ajj engine.

There're some exception as well.

1.For break instruction in loop, actually it can bypass multiple lexical scope which will let VM skip multiple EXIT instructions. To make compensation on this, the parser will generate special instruction for break and it will tell VM how many EXIT instructions it has passed, then VM can execute the GC exit based on this information.
2.For user registered objects/class/value, if it internally holds reference to other value inside of Ajj, then the user needs to explicitly change its scope if they need to. We have a special callback function for each registered object to do garbage collection.It will called when user defined objects needs to change the scope for its internal referenced Ajj value. User just need to simply iterate through each value internally it reference and call function ajj_value_move on it.

# TODO: Add more section
