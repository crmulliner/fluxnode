# Platform

Documentation for the FileSystem API.

Most of the code is either directly copied from or based on: https://github.com/nkolban/duktape-esp32.git
under the Apache 2.0 license.

## Methods

- [close](#closefd)
- [fsSizeTotal](#fssizetotal)
- [fsSizeUsed](#fssizeused)
- [fstat](#fstatfd)
- [listDir](#listdir)
- [open](#openpathflags)
- [read](#readfdbufferbufferoffsetmaxreadpos)
- [stat](#statpath)
- [unlink](#unlinkpath)
- [write](#writefdbufferoffsetlength)
- [write&nbsp;](#write&nbsp;fdstring)

---

## close(fd)

Close file descriptor.

- fd

  type: int

  file descriptor

```

```

## fsSizeTotal()

Get total size of the flash filesystem.

**Returns:** size in bytes of the filesystem

```

```

## fsSizeUsed()

Get used number of bytes of the flash filesystem.

**Returns:** bytes used of the filesystem

```

```

## fstat(fd)

Stat a file descriptor but inly return size.

- fd

  type: int

  file descriptor

**Returns:** {'size': file_size}

```

```

## listDir()

List files in the filesystem.

**Returns:** array of strings (e.g., ['filename', 'otherfilename'])

```

```

## open(path,flags)

Open file.

- path

  type: string

  file path

- flags

  type: string

  flags ('r', 'w', 'a', ...)

**Returns:** file descriptor

```

```

## read(fd,buffer,bufferOffset,maxRead,pos)

Read from a file descriptor.

- fd

  type: int

  file descriptor

- buffer

  type: Plain Buffer

  buffer

- bufferOffset

  type: uint

  offset into the buffer

- maxRead

  type: uint

  max num bytes to read

- pos

  type: uint

  position in the file to read from (currently NOT supported)

**Returns:** number of bytes read

```

```

## stat(path)

Stat a file path but inly return size.

- path

  type: string

  filepath

**Returns:** {'size': file_size}

```

```

## unlink(path)

Unlink (delete) file.

- path

  type: int

  filepath

```

```

## write(fd,buffer,offset,length)

Write to file descriptor.

- fd

  type: uint

  file descriptor

- buffer

  type: Plain Buffer

  data

- offset

  type: uint

  offset (optional)

- length

  type: uint

  length (optional)

**Returns:** num bytes written

```

```

## write&nbsp;(fd,string)

Write string to file descriptor.

- fd

  type: uint

  file descriptor

- string

  type: string

  data

**Returns:** num bytes written

```

```

