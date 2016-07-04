A Jinja2 Template Engine Port to C
===========================

# Features.
1. Support nearly all Jinja2 syntax.
2. Provide very flexible extension API. User could extend the engine by adding new runtime value, object and class.
3. Multiple extensions to Jinja2, including multi-inheritance , multi-level inheritance , move value to outer scope and return statement.
4. Automatic garbage collection via scope based strategy.
5. Dynamic global variable value binding through Json file or template itself.
6. Fully UTF encoding support.
7. Small code base with less 12000 lines of C code. Design for embedding.

# Not Supported Jinja2 Features
1.Loop recursive keyword is not supported.

2.Tuple is not supported.Tuple syntax is supported but it is automatically convert to list.
```
{% set MyTuple = (1,2,3) %}
```
is essentially same as
```
{% set MyTuple = [1,2,3] %}
```

3.One argument test function invoke without parenthesis is not supported.So you cannot write
```
{% if 2 is dividable 3 %}
```
but need to write as
```
{% if 2 is dividable(3) %}
```
4.Whitespace control is not optional. But the Jinja2 syntax is supported but takes no effect.
```
 {% do SomeThing %}
```
is same as
```
 {% do SomeThing -%}
```

5.Include instruction doesn't support options supported by Jinja2.
```
{% include 'some' ignore missing %}
{% include 'some' ignore missing with context %}
{% include 'some' ignore missing without context %}
```
is not supported. The include statement has also been extended . More detail will be reviewed in later section.

6.Line statements is not supported. The only supported 3 types of directives are :
```
{# This is a comment #}
{{ This something will be evaluated and output }}
{% This is a Jinja2 statement %}
```

7.Convert string is not *ONLY* operated by using ~ sign but also can be performed by using + sign.
```
{# Valid string concatenation #}
{{ 'Hello' + 'World' }}
{# Same as the following line #}
{{ 'Hello' ~ 'World' }}
```

8.The macro object is not supported inside of macro block.
```
{% macro MyMacro() %}
{# Invalid, macro is not supported in the macro scope #}
{{ macro.name }}
{% endmacro $}
```
But list of builtin variables are available in template.
+ `__func__`: represent the name of current function/macro/block/callable

+ `__argnum__`:represent the number of arguments for this function/macro/block/callable
+ `vargs`: same with python's vargs.
+ `caller`: name of the caller function
+ `self`: this Jinja2 object

9.Scopped block is supported through different syntax but not by tag a "scoped" keyword.

In old Jinja2, you can have such code:
```
{% set outer_var = [] %}
{% block MyBlock scoped %}
  {# Now you can use outer_var #}
{% endblock %}
```
But in AJJ, since each block is actually compiled into a function, you have to use the following syntax:

```
{% set outer_var = [] %}
{% block MyBlock(outer_var) %}
  {{ outer_var }}
{% endblock %}
```

As you can imagine, the outer scope variable is just passed to that block as normal function arguments.

10.Keyword function argument is not supported. It is a python thing and I don't think it will hurt all the template writer. But omit it for me is that I could design a simpler virtual machine. So you are not allowed to call a function like this:
```
{# NOTE: this is invalid should be like foo(1234,345) #}
{{ foo( arg1=1234,arg2 = 345 ) }}
```

11.Default value for macro arguments _must_ be constant value or evaluated to constant.

```
{% macro MyMacro( default_value = -100 , default_list = [1,2,3,4] ) %}
{% endmacro %}
```
A constant value is :
+ A constant number
+ A constant string
+ A constant boolean
+ A list that all elements are constant
+ A map that all elements are constant
```

So following value like 
```
{# Constant List #}
[1,2,3,4, [5,6,7]]
{# Constant Map #}
{ "A" : [1,2,3] }
```

# Language Extension to Jinja2
1. Flexible way to specify model file for template. User is allowed to specify model by 1) json file 2) template include statement.
To use json file to provide model , suppose you have prepared such json file: my_model.json

+ Json file

```
{ "MyVariable" : "HelloWorld" }
```

Now suppose you have such template requires a variable called MyVariable.
The template file name is my_temp.html:
```
{% for x in MyVariable %}
{{ x }}
{% endfor %}
```
When you want to render this template in another template, you just need to have such code:
```
{% include 'my_temp.html' json 'my_model.json' %}
```
Which will render your template using given JSON file as model.

+ Template file

User could use set statement nested inside of the include statement to set up the model for rendering the target template

```
{% include 'my_temp.html' %}
{% set MyVariable = 'HelloWorld' %}
{% endinclude %}
```

2.AJJ supports multiple inheritance.
```
  {% extends 'Template1' %}
  {% extends 'Template2' %}
```
Each extended template will be rendered in the declaration order.

3.AJJ supports multiple level inheritance.
Suppose you have template , a.html
```
{# Something interesting #}
```

And then you have template b.html
```
{% extends 'a.html' %}
```
Finally you have template c.html
```
{% extends 'b.html' %}
```

So basically b inherited from a and then c inherited from b. This is also supported.

5.One subtle thing for AJJ is that it allows you to do inheritance dynamically.Actually the extends statement accepts a variable instead of constant string. For AJJ , any inheritance chain is constructed during the runtime and not by the parsing/compile time. This enable extremely powerful inheritance functionality. Since for a specific template, although it has been compiled into bytecode , but you could still modify its base/extended base on the fly.

```
{% extends var %}
```

In this code, var is a variable. If the var's value changes the extends evaluated value will be modified accordingly.

6.Move Value to outer scope. In Jinja2, we have no way to let the variable in outer scope to access value in inner scope because the scope rules. User may need to bypass it using array + do scope. Now in AJJ, a special block is provided to do this.

```
  {% set OuterVar = None %}
  {% with InnerVar = [] %}
    {% move OuterVar = InnerVar %}
  {% endwith %}
```
The above code will move the value of InnerVar to OuterVar.

7.Return statements
Although it is questionable, but have a return statements may be useful for some tasks.

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
This code implement a macro that calculates the sum of an array and it will output 15.

8.Flexible extension API. AJJ is designed to be a library and its solo goal is make user who wants to embed Jinja2 template script engine into whatever host environment feel easy and comfortable. So AJJ provides lots of API for user to extend the AJJ runtime. It allows you to register global function, filter, pipe and test written in C. Also AJJ allows you register global variable and can be assigned with different value , including number, boolean , string and objects. Lastly, since AJJ is actually simulating the Python environment for Jinja2, it allows you register a new type of class using C API. With this user could write a class/object in C code and then instantiated it and use it in template.

# How does AJJ work ?
It turns out porting Jinja2 is not a easy task simply because from the point of porting Jinja2, it actually means porting the Python environment PLUS the Jinja2 front end. AJJ itself is designed as a very traditional script engine , ( do not think it as yet another template engine in any script language, but a python/ruby/perl implementation ). The whole work flow is as follow : User's template source code will be parsed into a byte code sequence initially, then these byte codes will be sent to a peephole optimizer to do constant folding + conditional jump elimination . To render the template, the final byte code sequence will be sent to then virtual machine for execution.

# Garbage Collection
GC is always a very important part for any script language. However, we design a very simple GC for Jinja2. Basically it is because Jinja2 has a very strict scope rules for us. In Jinja2, any local variable cannot *ESCAPE* from its lexical scope. To avoid complicated engineering for a mark&swap based GC algorithm. We simply assign each lexical scope a pool , which we call gc-scope. This gc-scope *OWNS* all the object on its corresponding scope. Because this scope owns all the memory for its corresponding scope, so when a lexical scope exits, we can just delete the memory owned by this scope and we won't leak any memory. As you may ask, what if the memory is referenced by other objects in other scope ? To address this problem, we need to know that only the scope has longer life cycle that holds a memory owned by a shorter life cycle gc-scope object will cause problem. In AJJ, every heap allocated memory is always owned by a scope and each scope has a life cycle tag. The larger the tag, the shorter the life cycle of this gc-scope is. Therefore, every time an assignment is happened, we could just check the gc-scope life cycle tag. If the destination's gc-scope life cycle is longer than the source object. A lift operation will be triggered, which is basically moves the object owned by shorter life cycle gc-scope to longer life cycle gc-scope. As you can guess, the global variable has longest life cycle gc-scope. To accurate create and destroy gc-scope, the parser will emit byte code for virtual machine to create and destroy gc-scope. The reason is because when virtual machine starts to execute the code, the lexical scope information is lost. With this design , we still have some exceptions:
1. AJJ allows `break` and `continue` statement. However this statement will SKIP these generated gc-scope instruction since they jump from inner lexical scope to outer scope.
2. AJJ allows `return` statement. Similar to situation 1, the instruction may be omitted.
To compensate these situations, parser will actually generate byte code that tells virtual machine how many gc-scope needs to be destroyed to maintain the states consistent. Virtual machine will do compensation accordingly.

This GC algorithm also make user who tries to write a extension have to cooperate with AJJ runtime. Any user defined objects will have a internal callback function which will be called when the corresponding gc-scope for this user defined object changes. Why user needs to care about this ? It is because if user defined objects internally have pointer pointed to any piece of memory in AJJ runtime, this pointer's pointed object's gc-scope may needs to be modified as well. Therefore, user needs to ensure each objects referenced internally in this object also has correct gc-scope. This will be covered more in the tutorial.

# Version
Alpha 0.0.1
