Source: ref-balatro/{cardarea,card}.lua
Last reviewed: 2026-05-01

# 02 - CardArea / Card

`CardArea` is the container; `Card` is the entity.
Both inherit from `Moveable`.

## CardArea

Core fields:

- `cards[]`
- `highlighted[]`
- `config.type` (`deck/hand/play/joker/consumeable/...`)

Core responsibilities:

- `align_cards()` for layout by area type
- `emplace/remove/draw_card_from` for movement between areas
- `parse_highlighted()` for hand evaluation and HUD updates

MVP-critical layout formulas:

- hand arc: `cardarea.lua:692-722`
- play line: `cardarea.lua:787-810`

## Card

A card is a layered visual object (`shadow/front/back/center`),
not a single sprite.

Flip animation uses `pinch` + VT recovery.
Edition/seal/sticker effects are shader multi-pass overlays.

## Port checklist

- keep: CardArea layout formulas and highlight ownership
- keep: Card flip state and layered sprite model
- rewrite: UIBox button wiring
- defer: advanced VFX behavior