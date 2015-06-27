## binary -- handling of binary data

The module contains functions to read and write binary data, encode it in textual form and decode it back.  

### Index
namespace [bin](#bin)

Functions:
- [read](#read)(_source_: io::Stream, _dest_: array&lt;@T&lt;int|float|complex&gt;&gt;, _count_ = 0) => int
- [unpack](#unpack)(_source_: io::Stream, _dest_: array&lt;int&gt;, _size_: enum&lt;byte,word,dword&gt;, _count_ = 0) => int
- [pack](#pack)(invar _source_: array&lt;int&gt;, _dest_: io::Stream, _size_: enum&lt;byte,word,dword&gt;, _count_ = 0) => int
- [write](#write)(invar _source_: array&lt;@T&lt;int|float|complex&gt;&gt;, _dest_: io::Stream, _count_ = 0) => int
- [get](#get1)(invar _source_: array&lt;int&gt;|string, _what_: enum&lt;byte,ubyte,word,uword,dword,udword,qword,uqword&gt;, _offset_: int) => int
- [get](#get1)(invar _source_: array&lt;int&gt;|string, _what_: enum&lt;float&gt;, _offset_: int) => float
- [get](#get2)(invar _source_: array&lt;int&gt;|string, _what_: enum&lt;bits&gt;, _offset_: int, _count_: int) => int
- [set](#set1)(_dest_: array&lt;int&gt;, _what_: enum&lt;byte,ubyte,word,uword,dword,udword,qword,uqword&gt;, _offset_: int, value: int)
- [set](#set1)(_dest_: array&lt;int&gt;, _what_: enum&lt;float&gt;, _offset_: int, _value_: float)
- [set](#set2)(_dest_: array&lt;int&gt;, _what_: enum&lt;bits&gt;, _offset_: int, _count_: int, _value_: int)
- [encode](#encode)(_str_: string, _codec_: enum&lt;base64,z85,hex&gt;) => string
- [decode](#decode)(_str_: string, _codec_: enum&lt;base64,z85,hex&gt;) => string

<a name="bin"></a>
### Functions
<a name="read"></a>
```ruby
read(source: io::Stream, dest: array<@T<int|float|complex>>, count = 0) => int
```
Reads *count* elements from *source* to *dest*. If *count* is zero, or greater than *dest* size, *dest* size is assumed. Returns the number of elements actually read

**Errors:** `Param` when *source* is not a file stream or is not readable
<a name="unpack"></a>
```ruby
unpack(source: io::Stream, dest: array<int>, size: enum<byte,word,dword>, count = 0) => int
```
Reads *count* chunks of size *size* from *source* into *dest* so that each chunk corresponds to a single *dest* element (with possible widening). If *count* is zero, or greater than *dest* element size, *dest* element size is assumed. Returns the number of chunks actually read

**Errors:** `Param` when *source* is not a file stream or is not readable
<a name="pack"></a>
```ruby
pack(invar source: array<int>, dest: io::Stream, size: enum<byte,word,dword>, count = 0) => int
```
Writes *count* chunks of size *size* to *dest* so that each *source* element corresponds to a single chunk (with possible narrowing). Returns the number of chunks actually written

**Errors:** `Param` when *dest* is not a file stream or is not writable
<a name="write"></a>
```ruby
write(invar source: array<@T<int|float|complex>>, dest: io::Stream, count = 0) => int
```
Writes *count* elements from *source* to *dest*. If *count* is zero, or greater than *dest* size, all *dest* data is written. Returns the number of elements
actually written

**Errors:** `Param` when *dest* is not a file stream or is not writable
<a name="get1"></a>
```ruby
get(invar source: array<int>|string, what: enum<byte,ubyte,word,uword,dword,udword,qword,uqword>, offset: int) => int
get(invar source: array<int>|string, what: enum<float>, offset: int) => float
```
Reads value described by *what* from *source* at the given byte *offset*

**Errors:** `Index::Range` when *offset* is invalid
<a name="get2"></a>
```ruby
get(invar source: array<int>|string, what: enum<bits>, offset: int, count: int) => int
```
Reads *count* bits from *source* at the given bit *offset*

**Note:** *count* must not exceed 32
**Errors:** `Index::Range` for invalid *offset*, `Param` for invalid *count*
<a name="set1"></a>
```ruby
set(dest: array<int>, what: enum<byte,ubyte,word,uword,dword,udword,qword,uqword>,offset: int, value: int)
set(dest: array<int>, what: enum<float>, offset: int, value: float)
```
Writes *value* described by *what* to *dest* at the given byte *offset*

**Errors:** `Index::Range` when *offset* is invalid
<a name="set2"></a>
```ruby
set(dest: array<int>, what: enum<bits>, offset: int, count: int, value: int)
```
Writes *count* lower bits of *value* to *dest* at the given *offset*

**Note:** *count* must not exceed 32
**Errors:** `Index::Range` for invalid *offset*, `Param` for invalid *count*
<a name="encode"></a>
```ruby
encode(str: string, codec: enum<base64,z85,hex>) => string
```
Returns *str* encoded with the given *codec*.

**Note:** For Z85 codec, *str* size must be a multiple of 4
**Errors:** `Bin` when the above does not hold
<a name="decode"></a>
```ruby
decode(str: string, codec: enum<base64,z85,hex>) => string
```
Returns *str* decoded with the given *codec*

**Errors:** `Bin` when *str* is not a valid *codec*-encoded string