# multithreaded-wordcount

```
cat lorem.txt | tr -cs '[:alnum:]' '\n' | tr '[:upper:]' '[:lower:]' | sort | uniq -c | sort -n -r
```
