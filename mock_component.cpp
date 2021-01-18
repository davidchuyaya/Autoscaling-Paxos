//
// Created by David Chu on 1/11/21.
//

#include "mock_component.hpp"

mock_component::mock_component(const int argc, const char**argv) {
	printUsage(argc < 2);

	const std::string& component = argv[1];
	mock* mockComponent;

	if (component == "client") {
		if (argc == 2)
			mockComponent = new mock(false);
		else if (argc == 3)
			mockComponent = new mock(true, argv[2]);
		else
			printUsage();
		mockComponent->client();
	}
	else if (component == "batcher") {
		if (argc == 2)
			mockComponent = new mock(false);
		else if (argc == 3)
			mockComponent = new mock(true, argv[2]);
		else
			printUsage();
		mockComponent->batcher();
	}
	else if (component == "proposer") {
		printUsage(argc < 3);
		const std::string& connectingTo = argv[2];
		if (connectingTo == "batcher") {
			mockComponent = new mock(false);
			mockComponent->proposerForBatcher(strcmp(argv[3], "true"));
		}
		else if (connectingTo == "proxy_leader") {
			mockComponent = new mock(true); //no server address, just acceptorGroupId
			std::vector<std::string> acceptorGroupIds;
			for (int i = 2; i < argc; ++i)
				acceptorGroupIds.emplace_back(argv[i]);
			mockComponent->proposerForProxyLeader(acceptorGroupIds);
		}
		else
			printUsage();
	}
	else if (component == "proxy_leader") {
		printUsage(argc < 3);
		const std::string& connectingTo = argv[2];
		if (connectingTo == "proposer") {
			printUsage(argc != 5);
			mockComponent = new mock(false, argv[3]);
			mockComponent->proxyLeaderForProposer(argv[4]);
		}
		else if (connectingTo == "acceptor") {
			printUsage(argc != 4);
			mockComponent = new mock(true, argv[3]);
			mockComponent->proxyLeaderForAcceptor();
		}
		else if (connectingTo == "unbatcher") {
			printUsage(argc != 5);
			mockComponent = new mock(true, argv[3]);
			mockComponent->proxyLeaderForUnbatcher(argv[4]);
		}
		else
			printUsage();
	}
	else if (component == "acceptor") {
		printUsage(argc != 3);
		mockComponent = new mock(true);
		mockComponent->acceptor(argv[2]);
	}
	else if (component == "unbatcher") {
		if (argc == 2)
			mockComponent = new mock(false);
		else if (argc == 3)
			mockComponent = new mock(true, argv[2]);
		else
			printUsage();
		mockComponent->unbatcher();
	}
	else
		printUsage();
}

void mock_component::printUsage(const bool ifThisIsTrue) {
	if (!ifThisIsTrue)
		return;

	printf("Usage: One of the following...\n\n");
	printf("Client sender: ./mock_component client <BATCHER_ADDRESS>\n");
	printf("Client receiver: ./mock_component client\n");
	printf("Batcher sender: ./mock_component batcher <PROPOSER_ADDRESS>\n");
	printf("Batcher receiver: ./mock_component batcher\n");
	printf("Proposer sender: ./mock_component proposer proxy_leader <ACCEPTOR_GROUP_IDS>...\n");
	printf("Proposer receiver: ./mock_component proposer batcher <IS_LEADER>\n");
	printf("Proxy leader receiver for proposer, where the proposer expects 1 acceptor group: ./mock_component proxy_leader proposer <PROPOSER_ADDRESS> <ACCEPTOR_GROUP_ID>\n");
	printf("Proxy leader for acceptor: ./mock_component proxy_leader acceptor <ACCEPTOR_ADDRESS>\n");
	printf("Proxy leader sender for unbatcher: ./mock_component proxy_leader unbatcher <UNBATCHER_ADDRESS> <CLIENT_ADDRESS>\n");
	printf("Acceptor: ./mock_component acceptor <ACCEPTOR_GROUP_ID>\n");
	printf("Unbatcher sender: ./mock_component unbatcher <CLIENT_ADDRESS>\n");
	printf("Unbatcher receiver: ./mock_component unbatcher\n");
	exit(0);
}

int main(const int argc, const char** argv) {
	INIT_LOGGER();
	mock_component m(argc, argv);
}