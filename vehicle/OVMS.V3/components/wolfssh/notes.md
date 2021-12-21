wolfssh notes
=============

coding standard
---------------

1. Exceptions are allowed with good reason.

2. Follow the existing style.

3. Try not to shorthand variables, except for ijk as indicies.

4. Lengths of arrays should have the array name followed by Sz.

5. Single return per function.

6. Check all incoming parameters.

7. No gotos.

8. Check all return codes. It feels a little tedious, but the preferred method
is running checks against success. This way if a function returns an error, the
code will drop to the end.

```
    ret = functionCall(parameter);
    if (ret == SUCCESS)
        ret = secondFunctionCall(otherParameter);
    if (ret == SUCCESS)
        ret = thirdFunctionCall(aParameter, anotherParameter);
    cleanUp();
    return ret;
```


