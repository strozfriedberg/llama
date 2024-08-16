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
