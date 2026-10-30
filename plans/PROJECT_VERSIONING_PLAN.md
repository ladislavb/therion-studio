# Project Versioning and Backup Plan

**Status:** Implementation in progress — local checkpoints, settings, restoration from the right File sidebar, private GitHub repository creation and explicit one-way plaintext uploads through `gh` are available; pre-compilation checkpoints, encrypted backups, remote restoration and multi-computer synchronization remain pending  
**Audience:** Therion Studio engineers  
**Post-read action:** Implement opt-in local project history with optional private GitHub backup, without changing an existing project Git repository or exposing project data unintentionally.

## 1. Goal and boundaries

Add automatic, per-project checkpoints so users can return to an earlier saved state. The feature is disabled by default and must not create repositories, read project contents for backup, or contact GitHub until the user enables it for that project.

Local version history is the foundation. A project may additionally mirror its history to a dedicated private GitHub repository. The public Therion Studio source repository is never a destination for cave-project data. Automatic synchronization is intended to remove routine pull/push actions, not to silently resolve conflicting edits.

This feature does not encrypt the active project files or local history. Optional client-side encryption protects only the GitHub copy. The local project and local history remain readable on the user's machine.

## 2. Project opt-in and repository isolation

- Versioning is enabled separately for each project and starts disabled.
- Identify the versioned project using Therion Studio's existing project root. If no project root is set, require the user to choose one; do not infer it from the active file's parent directory.
- Enabling versioning creates or selects an application-managed local history store separate from the project directory. Assign an application-generated project ID and keep its mapping to the canonical root in application settings, never in the project tree.
- Never initialize, rewrite, or repoint the project's existing `.git`, branches, remotes, hooks, or history. Existing Git state is not owned by this feature.
- GitHub backup is a separate opt-in configured for that project. It uses a dedicated private repository, created or selected by the user; it must not reuse the application repository or an arbitrary existing project remote. Verify the repository is private before any upload and reject public repositories. Use a generic, non-identifying default repository name; do not derive it from the cave or project name.
- Authenticate once through the system browser. Store access credentials only in the operating system's secure credential store. Do not put tokens in project files, Git history, logs, or the remote repository.
- Disabling the feature stops new checkpoints and synchronization but does not delete local history or remote data. Deletion must be an explicit, separately confirmed action.

## 3. Versioned files and generated-output policy

A checkpoint represents the whole project root subject to explicit ignore rules. Apply `.gitignore` files within the project tree plus Therion Studio's generated-output defaults; do not apply machine-global Git ignores or `.git/info/exclude`. The default policy includes Therion source/configuration files and project-local inputs, while excluding generated exports, configured Therion output directories, caches, and the feature's own state. Users may override the defaults for individual files or directories. Show the effective exclusions before the first snapshot and in project versioning settings.

Always exclude VCS metadata directories, including nested `.git` directories, and never follow symlinks outside the selected project root. The ignore rules must never exclude `.th`, `.th2`, or `.thconfig` source files by default. Generated Therion outputs such as compiled models and exported maps should be excluded by default, including files under conventional `output/` and `outputs/` directories. The exact patterns must be tested against representative project layouts so source assets and intentional inputs are not accidentally ignored.

Separately audit the public application repository's ignore rules and add precise defaults for generated Therion exports/output artifacts. Preserve intentional source fixtures and test data. The current repository already has a broad root-level Therion-related ignore pattern; review its effect before extending it rather than adding broader wildcard rules.

Checkpoints and restores operate only on files included by the project's effective ignore policy. A tracked/included file absent from a restored checkpoint is removed; ignored and unrelated untracked files are left untouched. Ignore settings and excluded paths must be visible in project versioning settings.

## 4. Checkpoint policy

- Detect saved file changes and group a burst of edits into one checkpoint after a configurable quiet/debounce period. Do not checkpoint every keystroke.
- Capture a candidate snapshot in temporary storage, verify that included files did not change while being read, and publish it atomically only after a complete successful capture. If a file changes mid-capture, discard the candidate and retry after the debounce period; never report a partial snapshot as successful.
- Therion compilation already saves all open dirty documents and cancels if a save fails. After that save succeeds and immediately before starting the compiler process, force a named checkpoint of the same on-disk inputs. Generated outputs from the compilation remain ignored unless the user explicitly includes them.
- Record generic checkpoint labels such as “Automatic checkpoint” and “Before Therion compilation”. Avoid putting cave names, file paths, source text, or other project-identifying content in commit messages.
- Keep history without automatic pruning. Any future cleanup or archival must be an explicit user action with a clear preview and warning.
- Offline or failed writes must not be reported as successful checkpoints. Keep local state authoritative and surface actionable failure status.

## 5. Encryption and key management

Encryption is an optional setting, disabled by default, and applies only to the remote GitHub representation. When enabled:

- Encrypt each file independently using a vetted authenticated-encryption implementation; encrypt the manifest as well so project paths and names are not exposed by the remote tree. Store encrypted blobs under opaque identifiers, never their original file names or paths.
- Never implement cryptographic primitives or invent a password-protected ZIP scheme. Select a maintained cross-platform library and document its algorithm and format/version before implementation.
- Keep the encryption key in the OS secure credential store. Require explicit one-time setup and provide a separate recovery key that the user can save outside the project and repository. There is no server-side key recovery; losing the key makes encrypted remote history unreadable. If secure key storage is unavailable, keep local checkpoints working but block encrypted remote writes; never silently downgrade to plaintext.
- Never push local plaintext history as part of an encrypted backup. The remote repository must contain only the encrypted representation, its non-sensitive format metadata, and generic commit labels.
- Reuse unchanged encrypted file objects where safe to limit storage growth. Use fresh nonces as required by the chosen authenticated-encryption scheme. Do not use deterministic encryption merely to enable deduplication.

If remote encryption is off, block the first plaintext upload until the user explicitly acknowledges that project contents will be readable to anyone with access to the private GitHub repository. Persist that consent per project. Provide a separate consent control; revoking it immediately blocks future plaintext uploads. Explain that revocation cannot erase plaintext already present in remote Git history; removal requires a separate explicit history-deletion operation.

## 6. GitHub synchronization and conflict handling

- On project open, before each push, and periodically while the project is open (target interval: about two minutes), check the dedicated remote for new history.
- On first connection, require an empty dedicated repository or one already initialized by this feature. If local checkpoints exist, upload the complete local history in the remote representation; never replace an unrelated non-empty repository.
- Expose explicit sync states: local-only/not configured, syncing, up to date, local changes queued, remote changes pending, conflict, and authentication/encryption/network error. Queue local history while offline and retry with bounded backoff; do not block editing or compilation on network availability.
- Automatically fetch and apply only a safe fast-forward while the project has no unsaved or uncheckpointed changes. Refresh open documents through the existing document lifecycle so external updates cannot silently overwrite editor buffers.
- If local and remote histories diverge, preserve both histories, stop remote writes, and report the conflict. Do not force-push, choose a side, or merge encrypted file contents automatically.
- Resume only after the user explicitly chooses the local or remote project state. Create a safety checkpoint first, then create a resolution commit whose parents preserve both divergent histories and whose tree uses the selected state; do not rewrite either history. Never perform a content merge automatically.
- A successful sync must not be claimed until the remote update has been confirmed. Authentication, encryption, storage, and network failures must remain visible and diagnosable.

## 7. Restore behavior

The delivered GitHub upload slice is explicitly narrower than full synchronization:
default-off plaintext opt-in, dedicated initially empty private repository,
manual File-sidebar action with a commit message and optional changed-only exit
prompt. The remote chain contains only upload snapshots, not local restore/safety
history. Foreign remote updates are rejected. Repository/account metadata and
upload preferences travel in project JSON; remote progress and credentials do not.
Downloading/restoring this remote chain remains a separate future slice.

The first version restores a whole checkpoint, not selected individual files. Show the selected checkpoint's timestamp and label and require confirmation with a replacement/deletion warning. Save modified editors and create a safety checkpoint before changing project files. Restore only tracked/included paths, removing included files absent from the target checkpoint while leaving ignored or unrelated untracked files untouched. Refresh/reopen affected documents using the normal project reload path. As requested, show the informational comparison after successful restoration in an OK popup with counts and file names in details. A detailed pre-write file preview is a later enhancement, not part of this delivered File-sidebar restoration slice.

Restoring a checkpoint must not delete the remote history or rewrite shared history. It creates a new local state representing the restored contents; if remote sync is enabled, publish that state as a new commit after normal conflict checks.

## 8. Security and reliability invariants

1. Default-off means no background scanning, Git initialization, credential prompt, or network activity before project opt-in.
2. The application repository and public repositories never receive project content through this feature.
3. An encrypted remote contains no plaintext source, file names, paths, or cave-specific commit messages in any reachable commit.
4. An unencrypted upload requires explicit per-project consent.
5. Existing project Git configuration and history are never modified.
6. Restore, conflict handling, and remote retries never silently discard local edits or history.
7. Local and remote failures are distinguishable; a checkpoint is not considered complete unless its local write succeeded.
8. Generated exports and outputs are excluded by default but can be deliberately included by the user.
9. Snapshot traversal never follows links outside the selected project root or includes nested VCS metadata.

## 9. Implementation phases

**Current local controls:** The Settings tab supports editing the root project
`.gitignore` with explicit save/reload and external-change protection. This changes
the real project file; nested ignore rules still apply. Optional timed checks
run in the background while the project is open, default to a five-minute
interval (configurable from 1–1440 minutes), and publish only changed snapshots.
They remain off unless enabled. Closing Settings does not stop the service;
closing the project does. Explicit local-history removal disables history and
requires confirmation before deleting the validated, locked app-managed store;
project files and project Git metadata are untouched. Toolbar navigation now
provides confirmed project restore with save-all, a mandatory safety checkpoint,
staged writes, rollback and an OK comparison popup. Interrupted restore leaves
a recovery pointer and blocks automatic capture until recovery. Restore refuses
links, file/directory transitions and collisions with currently ignored files.
Remaining local gaps are
event-debounced triggering/retries, pre-compilation capture, a complete effective-file
preview and nested-repository capture.

1. **Repository hygiene:** Audit the public repository's ignore behavior; add precise Therion generated-output rules and tests that verify intended source/test fixtures remain trackable.
2. **Local history foundation:** The isolated Git-backed store creates and lists local checkpoints without changing project Git metadata and excludes generated Therion outputs by default. Checkpoints use a temporary index, two fresh captures with matching tree hashes, a store lock, and atomic reference publication; a changed or failed capture does not advance history. Git environment overrides and attribute conversions cannot redirect the index or change source bytes. Each store is bound to its canonical project root, and new ignore rules also remove previously included files from subsequent checkpoints without deleting older history. Per-project application settings default to disabled, allocate a stable opaque ID only on opt-in, and retain the history mapping after disabling. Reading defaults or changing preferences alone does not initialize a Git repository. Settings contains a Backups and versioning tab scoped to the current project with the controls described above. Operations run outside the UI thread and errors remain visible. Backup controls apply immediately, independently of the general settings dialog's OK/Cancel. The GitHub section reports its unavailable status without offering nonfunctional actions.
3. **Security design:** Select and document the maintained authenticated-encryption format, encrypted manifest layout, credential-store integration, recovery-key flow, and behavior when encryption is disabled. Add round-trip and leakage tests before enabling remote writes.
4. **Private GitHub integration:** Add one-time browser authorization, private-repository create/select, encrypted/plaintext-consent gates, background fetch/push, offline queueing, and safe fast-forward/conflict handling.
5. **Cross-platform validation:** Test Linux, macOS, and Windows credential stores and filesystem behavior; verify offline operation, interrupted writes, concurrent edits, app restart, and recovery from a saved key.

## 10. Acceptance criteria

- A clean installation has versioning disabled and makes no GitHub requests.
- Enabling one project does not alter its existing `.git` metadata or any other project.
- A saved edit produces one debounced local checkpoint; a compile attempt produces a pre-compilation checkpoint even if compilation later fails.
- Generated Therion output and nested VCS metadata are absent from a default checkpoint, while source files and required project inputs are present; an external symlink target is never read.
- With encryption enabled, automated inspection of every reachable commit and repository object finds no canary source text, original file names/paths, project-specific repository names, or cave-identifying commit metadata; decrypting with the recovery key reconstructs each checkpoint, including deleted/older versions, exactly.
- With encryption disabled, the first upload is blocked until explicit plaintext consent is recorded.
- Remote changes fast-forward automatically only when safe; divergent histories are both retained, the user can resolve them by explicitly choosing a state, and no force-push occurs.
- Restoring a checkpoint requires confirmation, first saves modified documents and creates a safety checkpoint, removes included files absent from the target, preserves ignored files and prior history, and shows the resulting comparison in an OK popup.
- Token and encryption-key material are absent from project files, logs, process arguments, and repository objects.
