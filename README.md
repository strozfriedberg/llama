Llama is Lightgrep's Amazing Media Analyzer.

Llama 0.1.0 is *experimental* and should not be used for casework.

Llama is copyright (c) 2019-2024, Stroz Friedberg, LLC. Llama is available under version 2 of the Apache License. See [COPYING](COPYING) for details.

## Writing Llama Rules

If you're used to [writing YARA rules](https://yara.readthedocs.io/en/stable/writingrules.html), you'll have no problem learning how to write Llama rules.

Like YARA rules, Llama rules start with the `rule` keyword, followed by the rule name. The rest of the rule is enclosed in curly braces.

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

Though no sections are required for your rule to be valid, they must be in the order above. Sections in each rule are implicitly `AND`'d together.

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

The `hash` section is a place for you to filter by a file's hash value. Each comma-separated hash value is evaluated as belonging to the same file. Every value that is not comma-separated from the hash that came before it is evaluated as belonging to another file. This section only supports the `==` comparison operator.

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
* `filepath` - path to the directory containing the file, must be a double-quoted string
* `filename` - the name of the file, must be a double-quoted string

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

The `patterns` section is the place to define patterns (or if you're used to YARA, strings) that you want to look for with your rule. Each pattern must be assigned to an identifier, like so:

```
rule MyRule {
  grep:
    patterns:
      s1 = "foobar"
      s2 = "barfoo"
}
```

In the rule above, `s1` and `s2` are _pattern identifiers_ for `"foobar"` and `"barfoo"`. The next three sections describe pattern modifiers that you may use to specify attributes for your patterns. These modifiers may be combined for each pattern in any order.

##### Fixed strings

By default, patterns defined in Llama rules are evaluated as regex. This means that in a pattern such as "bad-domain.com", the period will be evaluated as a wildcard character. If you'd like for the pattern to be evaluated as a fixed string (so you don't have to escape the period), you can put the keyword `fixed` after the pattern assignment.

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
      s1 = "foo.bar" nocase
}
```

As you can see in the rule above, you can combine pattern modifiers for a single pattern.

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

##### Multiple pattern modifiers

To define multiple modifiers for a pattern, you must provide them in the following order:

```
fixed nocase encodings
```

#### Condition section

The `condition` section is the other subsection under the `grep` section. This section is where you can define the nature of the occurrences of your assigned patterns in a file for it to be considered a positive hit for your rule. The following functions are supported in the `condition` section.

##### any

The `any` condition function takes zero or more pattern identifiers as arguments and will evaluate to true if any of those patterns return a hit in the file.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: SMF" fixed
      s2 = "X-Mailer: Drupal" fixed
    condition:
      any(s1, s2)
}
```

The condition above is the equivalent of `any of (s*)` in a YARA rule. If no arguments are provided to `any`, it will evaluate to true if any of the patterns defined in the `patterns` section return a hit in the file. In that case, the YARA equivalent to Llama's `any()` would be `any of them`.

##### all

The `all` condition function takes zero or more pattern identifiers arguments. If you provide zero arguments, it will evaluate to true if there is at least one hit for each of the patterns defined in the `patterns` section. If you provide one or more arguments, it will evaluate to true if there is at least one hit for each of the patterns that you've specified as arguments.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: SMF" fixed
      s2 = "X-Mailer: Drupal" fixed
    condition:
      all()
}
```

This is the equivalent of `all of them` in a YARA rule.

##### count

The `count` function takes one pattern identifier as an argument and is used to verify the number of hits for a given pattern. A call to `count` must be followed by an integer comparison.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: SMF" fixed
      s2 = "X-Mailer: Drupal" fixed
    condition:
      count(s1) > 1
}
```

In the rule above, the `condition` will evaluate to true for a file if the file contains at least two hits for the `s1` pattern.

##### length

The `length` function is used to verify the length of a hit. It takes either one or two arguments. If one argument is provided, that argument must be a pattern identifier. If two arguments are provided, the first argument must be a pattern identifier, and the second argument must be an integer representing the occurrence. A call to `length` must be followed by an integer comparison.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: \w+"
    condition:
      length(s1) > 14 or length(s1, 2) > 17
}
```

For example, the call to `length(s1) > 14` will evaluate to true if any hits for the `s1` pattern are longer than 14 bytes. The call to `length(s1, 2) > 17`, on the other hand, will evaluate to true if the 2nd hit of the `s1` pattern is longer than 17 bytes.

##### offset

The `offset` function is used to verify the offset of a particular hit. Like `length`, `offset` can take one or two arguments. If one argument is provided, that argument must be a pattern identifier. If two arguments are provided, the first argument must be a pattern identifier, and the second argument must be an integer representing the `n`th occurrence. A call to `offset` must be followed by an integer comparison.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: \w+"
    condition:
      offset(s1) < 25 and offset(s1, 3) > 50
}
```

In this example, the call to `offset(s1) < 25` will evaluate to true if any of the hits for the `s1` pattern start before the 25th byte offset in the file. The call to `offset(s1, 3) > 50` will evaluate to true if the 3rd hit of the `s1` pattern starts after the 50th byte offset in the file.

##### count_has_hits

The `count_has_hits` function gives you the ability to verify that a given number of the patterns you've defined in the `patterns` section have more than one hit. This function takes zero or more arguments and must be followed by an integer comparison. If you provide  zero arguments, it will count the number of patterns from your `patterns` section that have a hit and compare it with the given integer. If you provide one or more arguments, it will count the number of patterns from the args that you've provided that have hits and compare it with the provided integer. In other words, `count_has_hits() == 4` is equivalent to YARA's `4 of them`, and `count_has_hits(s1, s2, s3) == 4` is equivalent to YARA's `4 of s*`.

```
rule Phishing {
  grep:
    patterns:
      s1 = "X-Mailer: SMF" fixed
      s2 = "X-Mailer: Drupal" fixed
      s3 = "billing.php" fixed
    condition:
      count_has_hits() > 1
}
```

The call to `count_has_hits` in the rule above will evaluate to true if at least two of the patterns in the `patterns` section has a hit.

## Rule error handling

If Llama encounters an unsupported character or invalid syntax in a rule, it will ignore all of the content since the last `rule` keyword and will skip to the next `rule` keyword.