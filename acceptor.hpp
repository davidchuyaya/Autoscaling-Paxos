//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP


class acceptor {
public:
    acceptor(int id);
private:
    const int id;
    void startServer();
    [[noreturn]] void listenToProposer(int socket);
};


#endif //C__PAXOS_ACCEPTOR_HPP
