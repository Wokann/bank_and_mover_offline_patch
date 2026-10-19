# Shared Bank/Mover features

[简体中文](README.zh-cn.md) · [Project](../README.md)

Bank and Mover compile the same local service and storage code separately. Native clients own their state machines, UI, Pokemon conversion and game saves; the service owns SD Bank data, configuration, tickets, gift replies and transactions.

The local service is not an HTTP listener, does not establish an official account session and does not upload a local Bank to the official server.

## Features

- Per-user Bank, client records, balances, passes and console locale; shared policy and gift assets.
- Current configuration at request boundaries, without retrospectively changing delivered results or commit/rollback decisions.
- Background Bank I/O and box screening using owned data; native clients retain callbacks, name correction and saves.
- Separate NDS/VC Pokemon rules and name lists. Custom conversion is off by default and does not bypass ordinary screening.
- Gift eligibility uses native Bank/game receipt fields, without a separate claim ledger.

## Configuration

The empty template is [configTemplate/templateForCopy/Bank](../configTemplate/templateForCopy/Bank); the complete example is [configTemplate/examples/Bank](../configTemplate/examples/Bank). The game does not read Markdown guides.

- [client.conf](../configTemplate/templateForCopy/Bank/client/readme_client.md): users, selection and migration.
- [server.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md): shared free/trial windows, prices, recharge and optional diagnostic logging.
- [user.conf](../configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.md): personal locale, currency, passes, trials, balances and custom conversion switches.
- [gift.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.md): switches, dates and gift assets.
- [checkPokemon.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.md): NDS/VC Pokemon screening.
- [checkName.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.md): NDS/VC name matching and exact exceptions.

## Data protection and cautions

Bank, journal, client records and source saves have dedicated protection; a configuration backup is not a Bank recovery decision. bankdata.bak retains the previous Bank before an actual change, while transaction.bak is the previous journal decision. Staging is not independently committed data.

Digital Gen5 uses x.bak/x.tmp beside the selected x.sav and retires them after confirmed saving/token cleanup. Ambiguous pending work is preserved; a checksum alone cannot prove game/Bank consistency.

Use independent regular files, not symbolic links, junctions or hard links. Stop both titles and back up before manual edits or migration; do not discard unfinished recovery evidence. See the [file inventory and protection policies](../configTemplate/templateForCopy/Bank/readme_fakeServer.md#files-written-and-protection-policies).

## Technical documentation

- [Service architecture, interfaces and data lifetime (Chinese)](docs/fakeServer/architecture.zh-cn.md)
- [Font coverage and memory behavior](docs/fonts.md)
- [Bank implementation](../bank/docs/code-analysis.md)
- [Mover implementation](../mover/docs/code-analysis.md)
