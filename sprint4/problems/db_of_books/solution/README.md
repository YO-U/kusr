# db_of_books

Консольное приложение `book_manager`: подключается к PostgreSQL по URL из первого аргумента, создаёт таблицу `books` при необходимости и обрабатывает JSON-команды из stdin (`add_book`, `all_books`, `exit`).

## Сборка

```bash
mkdir -p build && cd build
conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Исполняемый файл: `build/book_manager`.
