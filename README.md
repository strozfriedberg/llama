Llama is Lightgrep's Amazing Media Analyzer.

## Writing Llama Rules

If you're used to [writing YARA rules](https://yara.readthedocs.io/en/stable/writingrules.html), you'll have no problem learning how to write Llama rules.

Like YARA rules, Llama rules start with the `rule` keyword, followed by the rule name. The rest of the rule is surrounded with curly braces.

```
rule MyLlamaRule {

}
```

Below are the reserved keywords for Llama rules:

* `all`
* `and`
* `any`
* `blake3`
* `condition`
* `count`
* `count_has_hits`
* `created`
* `encodings`
* `file_metadata`
* `filename`
* `filepath`
* `filesize`
* `fixed`
* `grep`
* `hash`
* `id`
* `length`
* `md5`
* `meta`
* `modified`
* `name`
* `nocase`
* `offset`
* `or`
* `patterns`
* `rule`
* `sha1`
* `sha256`
* `signature`

## Rule sections

Llama rule sections are designated by their name followed by a colon. The following are reserved as Llama rule section names:

* `meta`
* `hash`
* `signature`
* `file_metadata`
* `grep`
* `patterns` (subsection of `grep`)
* `condition` (subsection of `grep`)

Though no sections are required for your rule to be valid, they must be in the order above. Sections in each rule are implicitly `AND`'d.

### Meta section

A Llama rule's `meta` section is a place for you to put metadata about the rule, such as the author, created date, or source URL. The fields in the meta section can be anything you want, as long as the values are double-quoted strings. Strings can contain newlines.

#### Example

```
rule MyRule {
  meta:
    author = "Me"
    description = "
      This is my rule
      and it's a great rule
    "
    source = "www.my-source.com"
}
```

### Hash section

The `hash` section is a place for you to filter by a file's hash value. Each comma-separated hash value is evaluated as belonging to the same file. Every value that is not comma-separated from the hash that came before it is evaluated as belonging to another file.

#### Example

```
rule MyRule {
  hash:
    md5 == "cafebabecafebabecafebabecafebabe", sha1 == "fab1efab1efab1efab1efab1efab1efab1efab1e"
    md5 == "babecafebabecafebabecafebabecafe" 
}
```

In this rule, the "cafebabe" and "fab1e" hashes will be implicitly `AND`'d. In other words, both the file's MD5 and SHA1 hash must match those hashes in order to match on that line. On the other hand, each record in the hash section is implicitly `OR`'d, meaning if that first line doesn't match, but the second one does, the section returns a match. So, if a file's MD5 hash is not `cafebabe`, but it does match `babecafe`, then the hash section will be evaluated as true.

### Signature section

The `signature` section allows you to filter files based on filetype. If you specify a file signature that you'd like to filter on, Llama will use Lightgrep to search for the magic bytes associated with that file type across the file system. The following fields are allowed in the `signature` section:

* `name`
* `id`

File signature names and IDs can be found in the `magics.json` file in the root of this repo. This section supports the boolean operators `AND` and `OR`. This section only supports the `==` comparison operator. Expressions may be grouped with parentheses.

#### Example

```
rule MyRule {
  signature:
    name == "Executable" or name == "ZIP Archive"
}
```

### File metadata section

The `file_metadata` section allows you to filter on the following file attributes:

* `created` - created time of the file, must be a ISO-8601-compliant datetime surrounded by double-quotes
* `modified` - modified time of the file, must be a ISO-8601-compliant datetime surrounded by double-quotes
* `filesize` - size of the file in bytes, must be an integer not surrounded by double-quotes
* `filepath` - path to the directory containing the file
* `filename` - the name of the file

Fields in this section may be combined with `AND` and `OR` boolean operators and expressions may be grouped with parentheses. This section supports the comparison operators `==`, `>=`, `<=`, `>`, and `<`.

#### Example:

```
rule MyRule {
  file_metadata:
    (created >= "2024-05-06" and filesize > 300000) or filename == "bad.exe"
}
```

### Grep Section

The `grep` section contains two subsections: `patterns` and `condition`.

#### Patterns section

The `patterns` section is the place to define patterns (or if you're used to YARA, strings) that you want to look for with your rule. These patterns must be assigned to an identifier, like so:

```
rule MyRule {
  grep:
    patterns:
      s1 = "foobar"
      s2 = "barfoo"
}
```

The next three sections describe pattern modifiers that you may use to specify attributes for your patterns. These modifiers may be combined for each pattern in any order.

##### Fixed strings

By default, patterns defined in Llama rules are evaluated as regex. This means that in a pattern such as "bad-domain.com", the period will be evaluated as a wildcard character. If you'd like for the period to be evaluated as a fixed string (so you don't have to escape the period), so you can put the keyword `fixed` after the pattern assignment.

```
rule myRule {
  grep:
    patterns:
      s1 = "bad-domain.com" fixed
}
```

##### Case-insensitive strings

By default, patterns defined in Llama rules are evaluated as case-sensitive. To disable this, add the `nocase` keyword after the pattern assignment.

```
rule myRule {
  grep:
    patterns:
      s1 = "foo.bar" fixed nocase
}
```

##### Pattern encodings

As a result of using Lightgrep for pattern search, Llama can support over 100 encodings. To search for a pattern with a specific encoding(s), use the `encodings` keyword followed by a comma-separated list of your encodings.

```
rule myRule {
  grep:
    patterns:
      s1 = "foobar" encodings=UTF-8,UTF-16LE
}
```

For a list of all supported encodings, run `lightgrep --list-encodings`.

