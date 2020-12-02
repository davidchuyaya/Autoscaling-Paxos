# Autoscaling-Paxos
A Paxos protocol that is capable of scaling dynamically.

## Setup
Our code is meant to run on AWS EC2 and contact an Anna cluster that is also running on EC2.

The following scripts were tested on **Ubuntu 18.04 x86**, with cmake/make optimized to use at least 4 cores. You should have an AWS account and plenty of money in it to spare (but the cost will be proportional to the throughput!).

#### Compiling locally
(Time estimate: 20 minutes) Run the following scripts:
```shell script
scripts/submodule_update.sh
scripts/install_dependencies_for_building.sh
scripts/download_protobuf.sh
scripts/install_protobuf.sh
```
and to build, choose from 
```shell script
scripts/build_debug.sh
```
or
```shell script
scripts/build.sh
```
Then you should see generated executables within the project folder, such as `acceptor`, which can be run from the command line.

Note: Running the executables at this point won't do anything, since Anna is not running yet and environment variables have not been set.

#### Optional: run Anna locally
Follow the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/building-anna.md) and the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/local-mode.md) to run Anna locally, just to get a taste of what it does. Maybe it'll help you debug.

### Setting up Anna on EC2
[Anna](https://github.com/hydro-project/anna) is a low-latency, eventually consistent, auto-scaling KVS acting as our system's router. Some Anna code is imported as a submodule from the [repo here](https://github.com/hydro-project/common).

The following steps are based on [instructions here](https://github.com/hydro-project/cluster/blob/master/docs/getting-started-aws.md). The scripts are not in a `.sh` file because they involve human interaction.

(Time estimate: 1 day) Before you start:
1. Buy a domain in Route53. Allow a day for it to be propagated in DNS. I followed the [instructions here](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/domain-register.html).
2. Request EC2 vCPU autoscaling limit to be raised from 32. I'm on the west coast, so I modified the [EC2 settings here](https://us-west-1.console.aws.amazon.com/ec2/v2/home?region=us-west-1#Limits:), but you can change the region to your liking. You'd request an increase for `Launch configurations` and `Auto Scaling groups`. I increased both to 200. Allow a day for this limit increase to be approved.

(Time estimate: 2 hours) Start an EC2 instance with plenty of compute power (I used a c5.2xlarge) to run the following scripts.

```shell script
sudo apt-get update

export HYDRO_HOME=~/hydro-project
mkdir $HYDRO_HOME
cd $HYDRO_HOME
git clone --recurse-submodules https://github.com/hydro-project/anna.git
git clone --recurse-submodules https://github.com/hydro-project/anna-cache.git
git clone --recurse-submodules https://github.com/hydro-project/cluster.git
git clone --recurse-submodules https://github.com/hydro-project/cloudburst.git
cd cluster

# kubernetes
curl -LO "https://storage.googleapis.com/kubernetes-release/release/$(curl -s https://storage.googleapis.com/kubernetes-release/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x ./kubectl
sudo mv ./kubectl /usr/local/bin/kubectl

# kops
curl -Lo kops https://github.com/kubernetes/kops/releases/download/$(curl -s https://api.github.com/repos/kubernetes/kops/releases/latest | grep tag_name | cut -d '"' -f 4)/kops-linux-amd64
chmod +x ./kops
sudo mv ./kops /usr/local/bin/

# AWS cli
sudo apt install -y python3-pip unzip
pip3 install awscli boto3 kubernetes
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install
```

At this point you will want to set up the AWS CLI.
```shell script
aws configure
```
Enter your access key ID, secret key, and the region you wish to use. We installed on us-west-1. You can set the access key & secret key up through IAM.

```shell script
# kops configuration
export AWS_ACCESS_KEY_ID=$(aws configure get aws_access_key_id)
export AWS_SECRET_ACCESS_KEY=$(aws configure get aws_secret_access_key)
aws iam create-group --group-name kops
aws iam attach-group-policy --policy-arn arn:aws:iam::aws:policy/AmazonEC2FullAccess --group-name kops
aws iam attach-group-policy --policy-arn arn:aws:iam::aws:policy/AmazonRoute53FullAccess --group-name kops
aws iam attach-group-policy --policy-arn arn:aws:iam::aws:policy/AmazonS3FullAccess --group-name kops
aws iam attach-group-policy --policy-arn arn:aws:iam::aws:policy/IAMFullAccess --group-name kops
aws iam attach-group-policy --policy-arn arn:aws:iam::aws:policy/AmazonVPCFullAccess --group-name kops
aws iam create-user --user-name kops
aws iam add-user-to-group --user-name kops --group-name kops
aws iam create-access-key --user-name kops
```

We need to create an S3 bucket for kops, but names can't conflict globally, so your bucket name can't be the same as mine. Substitute `<your bucket here>` with what you'd like to name your bucket. Note that the following script does some changes that are particular to setting up in us-west-1. Make changes so it runs in the region you want.
```shell script
aws s3api create-bucket \
    --bucket <your bucket here> \
    --region us-west-1 \
    --create-bucket-configuration LocationConstraint=us-west-1
aws iam create-service-linked-role --aws-service-name "elasticloadbalancing.amazonaws.com"
```

Turns out that EC2 instances do not come with SSH keys. We'll need to generate them; they're used in the cluster creation script later.
```shell script
# Set ssh key in ~/.ssh/id_rsa
cd ~/.ssh
ssh-keygen -o
```

```shell script
cd $HYDRO_HOME/cluster

# Change us-east-1 to us-west-1
grep -rl us-east-1 . | xargs sed -i 's/us-east-1/us-west-1/g'
grep -rl us-west-1a . | xargs sed -i 's/us-west-1a/us-west-1b/g'

# Rerun every time if you exit & login again to this EC2 instances
export HYDRO_HOME=~/hydro-project
export AWS_ACCESS_KEY_ID=$(aws configure get aws_access_key_id)
export AWS_SECRET_ACCESS_KEY=$(aws configure get aws_secret_access_key)
```

We're almost ready to run. Set the environment variables for the domain name you bought earlier at `<your domain here>`, and the same S3 bucket name you gave to kops earlier for `<your bucket here>`.
```shell script
# Rerun every time if you exit & login again to this EC2 instances
export HYDRO_CLUSTER_NAME=<your domain here>
export KOPS_STATE_STORE=s3://<your bucket here>

python3 -m hydro.cluster.create_cluster -m 1 -r 1 -f 0 -s 0
```
**Record the routing address that this script outputs.** Our scripts will need it to talk to Anna.

We have running instances of Anna! Note that `-f 0 -s 0` means that we're not running functions or scheduling, both of which are [cloudburst](https://github.com/hydro-project/cloudburst) specific.

Do remember that
1. You cannot remove values from Anna. We store routing information in an unordered set, so if you want to clear the routing information, you'd need to restart Anna between runs. Run the following **line by line** so you can edit the files manually, and change the minCount/maxCount to 0, then propagate the update:
```shell script
kops edit ig master-us-west-1b
kops edit ig memory-instances
kops edit ig misc-instances
kops edit ig routing-instances
kops update cluster --yes
```
2. Anna is expensive, so you might want to shut down Anna when you're not using it, using the method above.

### Running on an EC2 instance
We need EC2 instances to run components of Autoscaling Paxos.

We will use AMIs. Our executables depend upon dynamically linked libraries (such as protobuf and 0MQ), so we'd either have to `make install` them every time we boot up (which can take 10+ minutes), or package it into a system snapshot that's loaded on boot. Our goal is to scale in real time, so boot time is precious.

Follow the [instructions here](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/creating-an-ami-ebs.html#how-to-create-ebs-ami) to create an AMI. I used the [Ubuntu AMI locator](https://cloud-images.ubuntu.com/locator/ec2/) to help me find a Ubuntu 18.04 AMI with x86 and EBS backed storage in us-west-1. The ID of that AMI was `ami-00da7d550c0cbaa7b`. Once you launch that AMI from the EC2 console, run these scripts:
```shell script
scripts/install_dependencies_for_running.sh
scripts/download_protobuf.sh
scripts/install_protobuf.sh
```
Continue with the instructions above to create your custom AMI.

You'll need to figure out how to get the executables generated from builds into these instances. I stored them in S3 and used `wget` to download them.

Before you run the executables within any instance, be sure to set the environment variables. Substitute `<your Anna address here>` with the routing address that Anna outputted when you set it up earlier.
```shell script
export ANNA_ROUTING=<your Anna address here>
export IP=$(curl http://169.254.169.254/latest/meta-data/public-ipv4)
```

You can now run the executables! If only you knew how.

## Run Autoscaling Paxos

TODO

