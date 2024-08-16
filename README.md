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

Though no sections are required for your rule to be valid, they must be in the order above.

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