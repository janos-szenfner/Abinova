# UI04 diagnosis — `ui-drive --frame` wedge (split from blocked UI02)

Date: 2026-10-06. Method: `ui-drive --frame` under `xvfb-run -a` with
`WAYLAND_DISPLAY` unset + `GDK_BACKEND=x11` (FRZ07 pinning), `UI_DRIVE_TRACE=1`,
SIGQUIT probe used to capture phase + stack at the wedge. Two runs on the
drvwrap fixture (`test/wp/cov07/rich.abw` scratch copy) produced identical
signatures; a third run on `test/wp/BillOfRights.abw` served as a control.

## Verdict

Not a driver polling bug — the frame leg dies from a cascade of REAL product
defects, then the rescue machinery strands GLib locks so every subsequent
signal call deadlocks. Three distinct findings, in the order they hit:

### Finding 1 (the 99% CPU spin): non-convergent `fb_ColumnBreaker::breakSection` per ribbon format change

- The drvwrap fixture `rich.abw` converts to PDF headless in seconds with
  zero warnings — the doc's static layout converges fine.
- Under the drive, the CPU pinned at ~117% the whole leg. The trace shows
  why: inside the **Borders dropdown** (`_makeBordersPopover`,
  ap_UnixRibbon.cpp:10446 — the `GtkMenuButton`'s `GtkPopover`), each
  `_borderRow` button click invokes the `paraBorder` edit method →
  `FV_View::cmdParaBorder` → `PD_Document::changeStruxFmt` →
  `fl_DocListener::change` → `fl_BlockLayout::format`, and every such pass
  ends in
  `fb_ColumnBreaker::breakSection`'s 50-attempt retry loop without
  converging: `Abinova: section break did not converge after 50 re-break
  attempts; leaving layout as-is` (the LAY06 UT_WARNINGMSG).
- ~4 non-convergent breakSection calls per border click, ~25 warnings in
  ~40 s of leg time — the leg spends nearly all wall time inside the
  re-break loop, i.e. sustained ~100% CPU while making only slow progress.
  On a live session each such click is a multi-second main-thread stall —
  exactly the observed "window not responding / 99% CPU".
- Suspected mechanism (not fixed here): a layout fix-point oscillation.
  Paragraph borders feed line thickness via
  `fp_Line::calcBorderThickness`/`canDraw{Top,Bot}Border`, which depend on
  the *neighbouring* block's border state — a border change can flip line
  heights across a column/page boundary in a way that never settles within
  the 50-attempt cap. Confirming the needsRebreak reason (LAY06's
  instrumentation is DEBUGMSG-only) needs a debug build.
- Control: BillOfRights.abw frame leg produced **0** convergence warnings —
  the pathology is doc+interaction specific (rich.abw contains tables,
  borders-adjacent paragraphs, hdrftr).

### Finding 2 (deterministic SIGSEGV, product bug): unchecked downcast in `fp_Line::canDrawBotBorder`

Identical crash in both rich.abw runs, phase `interact GtkButton`:

```
fl_ContainerLayout::getPrev()                 <- this = bad ptr
fl_BlockLayout::canMergeBordersWithPrev()
fp_Line::canDrawBotBorder()                   fp_Line.cpp:680
fp_Line::calcBotBorderThick / calcBorderThickness
fp_Line::setContainer
fp_VerticalContainer::insertContainerAfter
fl_BlockLayout::getNewContainer / _stuffAllRunsOnALine / format
fl_BlockLayout::doclistener_changeStrux ... pt_PieceTable::changeStruxFmt
FV_View::cmdParaBorder  <- ap_EditMethods::paraBorder
AP_UnixRibbon::_invokeEditMethod <- _s_popover_em_clicked
```

Root cause at `src/text/fmt/xp/fp_Line.cpp:671-680`:

```cpp
fp_Container * pNext = pLast->getNextContainerInSection();
if(pNext == nullptr) return true;
fp_Line * pNextL = static_cast<fp_Line *>(pNext);   // unchecked!
...
fl_BlockLayout * pNextBlock = pNextL->getBlock();   // garbage on non-line
if(pNextBlock->canMergeBordersWithPrev())           // null/bad this -> SEGV
```

`getNextContainerInSection` (fp_Line.cpp:3676) skips ENDNOTE/FRAME/FOLDED
layouts but NOT table layouts — when the next in-section container is a
`fp_TableContainer`, the unchecked `static_cast<fp_Line *>` reads a garbage
`m_pBlock` and `canMergeBordersWithPrev`/`getPrev` faults on the bad `this`.
The sibling `canDrawTopBorder` (fp_Line.cpp:646-648) DOES guard:
`if(!pPrev || pPrev->getContainerType() != FP_CONTAINER_LINE) return true;`
— the bottom-border path is missing the same check, and additionally has
no `pNextBlock` null check.

Trigger: `paraBorder` on a paragraph whose next section container is a
table — i.e. click a preset in the ribbon **Home > Borders** dropdown on a
paragraph adjacent to a table. Real user-facing crash, deterministic.

### Finding 3 (the unrecoverable wedge): rescue-longjmp strands GLib locks

After crash-1's rescue (`siglongjmp` out of a live `g_signal_emit_by_name`):

- The NEXT button click deterministically faulted a second time inside
  libgobject: `focus_in_event` → `FV_View::focusChange` → `refreshRibbon`
  → `EV_UnixMenu::_refreshMenu` → `g_simple_action_set_state` →
  `g_object_notify` → `g_signal_emit` → SEGV in `libgobject +0x44128`
  (corrupted emission/handler state — identical frame in both runs).
- From then on every `g_signal_connect_data` (e.g. inside a widget ctor,
  and inside the emission probe's `gtk_button_new`) blocks on a futex —
  captured live by SIGQUIT in `phase: interact GtkButton`:
  `... g_object_new -> GtkWidget ctor -> g_signal_connect_data ->
  glib+0xb666a -> syscall`.
- `glib_emission_alive()`'s own `gtk_button_new` wedged the same way;
  its 5 s alarm fired, `post_rescue_check` reported
  `signal emission dead after rescue` and the process `_exit(3)`ed —
  i.e. the leg self-terminates now, but only because the probe exists.
- Control run (BillOfRights) confirms the class: no prior fault at all and
  it still died in a lock — `interact GtkListView` (the drive emits
  `activate`) wedged inside `g_variant_get_child_value -> g_bit_lock`
  during GTK listitem construction; 20 s alarm, rescue, then the same
  emission-dead exit. A GVariant bit-lock can also be left held by a
  non-local exit out of variant-printing code.

## Suspected fixes (for UI05)

1. `fp_Line::canDrawBotBorder`: add the `FP_CONTAINER_LINE` type check (and
   a `pNextBlock` null guard) mirroring `canDrawTopBorder` — kills the
   deterministic crash and probably the layout oscillation that feeds
   Finding 1's non-convergence (the border-merge decision currently runs
   on a garbage neighbour).
2. `fb_ColumnBreaker::breakSection` non-convergence on rich.abw: needs a
   debug-build repro to read needsRebreak's reason; likely the same
   border-merge feedback loop (border on -> line thicker -> line spills to
   next column -> merge recomputes differently -> repeat).
3. Driver robustness (optional): after any rescue the GLib state is
   unrecoverable — consider running the frame walk in a fork'd child, or
   treating the first rescued fault as end-of-leg, since every subsequent
   emit/connect is deadlock-prone by construction. The existing
   `post_rescue_check` already bounds the damage by exiting.

## Reproduction

```sh
cd src/wp/test
env -u WAYLAND_DISPLAY GDK_BACKEND=x11 UI_DRIVE_TRACE=1 \
    UI_DRIVE_DOC=/tmp/rich-scratch.abw \
    xvfb-run -a ./ui-drive --frame    # dies ~40 s in: crash cascade
kill -QUIT <pid>                      # dumps phase+stack at the wedge
```

Captured logs: `/tmp/ui04-frame.log`, `/tmp/ui04-frame2.log` (rich.abw,
identical signatures), `/tmp/ui04-bor.log` (BillOfRights control).
