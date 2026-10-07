# NetMessenger Design Diary

**Student Registration:** IT23569768  
**Module:** IE3010 - Network Programming  

---

## 1. Concurrency Model Decision

- **Chosen Approach:** POSIX Threads (`pthreads`)
- **Rationale:** Threading allows the server to handle multiple concurrent clients efficiently on modern multi-core systems while maintaining clean, shared state memory for user lists, chat rooms, and file transfer routing.

---

## 2. Personalisation Parameters

- **Port:** 15768
- **Node ID Tag:** NID:5697
- **Log File:** `netmsg_IT23569768.log`
- **Storage Path:** `./storage/IT23569768/`

---

## 3. Development Progress & Key Milestones

- [x] Initial repository structure, README, `.gitignore`, and Makefile setup.
- [ ] Base server setup (socket creation, bind on port 15768, listen).
- [ ] Multi-client handling & connection thread setup.
- [ ] Protocol command handling (`REGISTER`, `LIST`, `BCAST`, `PMSG`).
- [ ] Room management implementation (`JOIN`, `LEAVE`, `ROOMS`, `RMSG`).
- [ ] File transfer implementation (`SENDFILE`).
- [ ] Logging mechanism and disconnect state cleanup.
