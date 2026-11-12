# Remotes that Flipper-IRDB does not have

[Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) is the main source of remotes for IRis. This folder is for
remotes that are **not** in it, starting with brands and set-top boxes sold in Argentina. It holds no copy of the
database: a remote that Flipper-IRDB already has does not belong here.

The folder is empty for now. Nothing here was invented or guessed: a file is added only when it comes from a real
remote.

## Layout

The same as Flipper-IRDB, so both can be copied into `/irdb` on the card and the folders merge:

```
irdb-extra/
  TVs/Noblex/Noblex_<model>.ir
  Cable_Boxes/Telecentro/Telecentro_<model>.ir
```

## Adding a remote

1. Get the signals:
   - **From the remote itself.** Learn its keys on the device and use *Export to SD*; the file is written to
     `/iris/export/<category>/<remote>.ir` on the card.
   - **From codes found elsewhere.** A LIRC `.conf` file or a list of Pronto hex codes.
2. Convert and compare with a checkout of Flipper-IRDB:

   ```powershell
   python tools/ir_convert.py <file or folder> -o irdb-extra/TVs/<Brand> --irdb <Flipper-IRDB folder>
   ```

   A remote whose signals are all in one file of the database is reported with the name of that file and is not
   written. The others are written, and the report says how many of their signals the database already has.
3. Name the file `<Brand>_<model>.ir`, and name the keys as Flipper-IRDB does (`Power`, `Vol_up`, `Vol_dn`, `Ch_next`,
   `Ch_prev`, `Mute`), so that *Be Gone* finds the power key.
4. Try it from the card before committing it.

A remote that is good enough for this folder is also worth sending to Flipper-IRDB.

## Licence

Files in this folder are released as CC0 1.0, like Flipper-IRDB, so they can be contributed there.
