# NetMessenger Testing Record

Registration Number: IT23569768

| Test ID | Feature | Test | Expected Result | Actual Result | Status |
|---------|---------|------|-----------------|---------------|--------|
| T01 | Server Startup | Start server | Server listens on port 15768 | | |
| T02 | Registration | Register new username | Registration succeeds | | |
| T03 | Duplicate Username | Register same username twice | USERNAME_TAKEN error | | |
| T04 | User List | Send LIST | Connected users returned | | |
| T05 | Broadcast | Send BCAST message | Other clients receive message | | |
| T06 | Private Message | Send PMSG | Target user receives message | | |
| T07 | Join Room | Send JOIN | User joins or creates room | | |
| T08 | Leave Room | Send LEAVE | User leaves room | | |
| T09 | List Rooms | Send ROOMS | Available rooms returned | | |
| T10 | Room Message | Send RMSG | Room members receive message | | |
| T11 | File Transfer | Send SENDFILE | Complete file transferred | | |
| T12 | Invalid User | Message unknown user | Error returned | | |
| T13 | Disconnect | Kill client unexpectedly | Server remains running | | |
