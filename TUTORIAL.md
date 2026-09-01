# .PG Language Tutorial

## Getting Started

### Running a script
```
init.exe script1.pg        # Run a specific script
init.exe                   # Open the launcher menu
```

The launcher lets you:
- Browse folders to find `.pg` files
- Type `[B]` then a folder path to navigate
- Type a number to run a script or enter a folder
- Type `[U]` to go up one directory

---

## Language Basics

### Comments
```
-- This is a line comment
// This is also a line comment
/* This is a
   block comment */
```

### Variables
```
store[type="Local"]: "value" as myVar
store[type="Local"]: 42 as number
store[type="Local"]: true as flag
```

### Print
```
print "Hello World"
print "Value: ${myVar}"
print "Math: ${1 + 2}"
```

### String Interpolation
Use `${}` inside strings to insert variables or expressions:
```
store[type="Local"]: "Alice" as name
print "Hello, ${name}!"
print "2 + 2 = ${2 + 2}"
```

---

## Operators

### Arithmetic
| Operator | Description |
|----------|-------------|
| `+` | Add / concatenate strings |
| `-` | Subtract |
| `*` | Multiply |
| `/` | Divide |
| `%` | Modulo |

### Comparison
| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |

### Logical
| Keyword | Description |
|---------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |

---

## Control Flow

### If / Else
```
if x > 10 {
    print "Big"
} else {
    print "Small"
}
```

### While Loop
```
store[type="Local"]: 0 as i
while i < 10 {
    print "${i}"
    store[type="Local"]: i + 1 as i
}
```

### For Loop
```
for (store[type="Local"]: 0 as i; i < 10; i = i + 1) {
    print "${i}"
}
```

---

## Methods

Call methods on variables with `variable:Method[argument]`:

```
store[type="Local"]: "hello world" as msg

print ${msg:Upper[]}         -- HELLO WORLD
print ${msg:Length[]}        -- 11
print ${msg:Contains[hello]} -- true
print ${msg:Substr[5]}       -- hello
```

### Built-in Methods
| Method | Description |
|--------|-------------|
| `Upper[]` | Convert to uppercase |
| `Lower[]` | Convert to lowercase |
| `Length[]` | Get string length |
| `Contains[val]` | Check if string contains value |
| `Substr[n]` | Get first n characters |
| `GetJSON[arg]` | Parse JSON (stub) |

---

## File Operations

### Create a folder
```
createFolder "myFolder"
```

### Create a file
```
cf filename in["path"] script: {
    File content here
    Line 2
    Line 3
}
```

---

## Server Fetch

Download data from a URL:
```
store[type="FromServerToLocal"]: "https://api.example.com/data" as response
print "${response}"
```

---

## Libraries (Stub)

Import external libraries:
```
import[lib.MyLibrary]
import[lib.Included.WinForm]
```

Libraries are `.pglib` files (not yet implemented).

---

## Example: Complete Script

```
-- My first .pg program
store[type="Local"]: "Alice" as name
store[type="Local"]: 0 as score

for (store[type="Local"]: 0 as i; i < 5; i = i + 1) {
    store[type="Local"]: score + 10 as score
}

if score > 40 {
    print "${name} won with ${score} points!"
} else {
    print "${name} scored ${score} points."
}

createFolder "results"
cf result.txt in["results"] script: {
    Player: ${name}
    Score: ${score}
}
print "Results saved!"
```
