# Autoscaling-Paxos
A Paxos protocol that is capable of scaling dynamically.
Checkout our paper (work in progress) on [Overleaf](https://www.overleaf.com/read/bvnnzjbpxybc)!

## Before we begin
Our code is meant to run on AWS EC2 and contact an Anna cluster that is also running on EC2. Decide which AWS region you'd like to run things in. We'll use `<your AWS region>` throughout to refer to the it. Additionally, pick an AWS Availability Zone within that region, and replace any mentions of `<your AWS Availability Zone>` with it. We tested in region `us-west-1`, availability zone `us-west-1b`.

The following scripts were tested on **Ubuntu 18.04 x86**, with cmake/make optimized to use at least 4 cores. You should have an AWS account and plenty of money in it to spare (but the cost will be proportional to the throughput!).

Note that some steps will take **days**; you can complete them first [here](#setting-up-anna-on-ec2).

### Compile locally for testing
(Time estimate: 20 minutes) Run the following scripts:
```shell script
scripts/submodule_update.sh
scripts/install_dependencies_for_building.sh
scripts/download_protobuf.sh
scripts/install_protobuf.sh
scripts/install_prometheus.sh
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

### Optional: run Anna locally for testing
Follow the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/building-anna.md) and the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/local-mode.md) to run Anna locally, just to get a taste of what it does. Maybe it'll help you debug.

## AWS Setup
These steps can be run on your local computer. Remember to [install the AWS cli](https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2.html).

### Create an AWS user
Head to the [AWS IAM Console](https://console.aws.amazon.com/iam/home?region=us-west-1#/users) to create a new user with root access. Root access is not *necessary*, but we'll be doing a lot with this user so I didn't narrow down what permissions are actually needed. As such, leaking your user info (in a `git commit`, for example) is a bad idea.

1. Give it **Programmatic access**.
2. Click **Attach existing policies directly**, and give it **Administrator access**.

Finish creating the user, and write down the **access key ID** and the **secret key**. Now you can login by running
```shell
aws configure
```
and entering the access key ID and secret key.

#### Create a security group with all ports opened
To communicate with Anna, the EC2 nodes that we'll run on need to have some incoming/outgoing ports enabled. For simplicity, I'm going to enable all ports.

Run the following to create the security group:
```shell
aws ec2 create-security-group \
  --description none \
  --group-name paxos-security-group
```
Write down the security group ID that's generated.
Run the following to enable all incoming traffic:
```shell
aws ec2 authorize-security-group-ingress \
  --group-name paxos-security-group \
  --protocol all \
  --cidr 0.0.0.0/0
```

#### Create a key pair
To SSH into EC2 machines for debugging, we need generate a key pair that authenticates us.

Run the following to create the key pair:
```shell
aws ec2 create-key-pair \
  --key-name paxos-key \
  --query 'KeyMaterial' \
  --output text > paxos-key.pem
chmod -R 400 paxos-key.pem
```
The file `paxos-key.pem` should be generated locally with the private RSA key. You will need this file to SSH into the machines later; don't lose it or leak it.

#### Create a S3 bucket for executables
Create a bucket for Github Actions that defaults to read-only for the public.

First, create a JSON file containing the read-only policy below, replacing `<your executable S3 bucket>` with a unique name. We will refer to this file as `<your read policy file>` from now on.
```json
{
  "Version":"2012-10-17",
  "Statement":[
    {
      "Sid":"PublicRead",
      "Effect":"Allow",
      "Principal": "*",
      "Action":["s3:GetObject"],
      "Resource":["arn:aws:s3:::<your executable S3 bucket>/*"]
    }
  ]
}
```


Run the following, replacing `<your executable S3 bucket>` and `<your read policy file>`:
```shell
aws s3api create-bucket \
    --bucket <your executable S3 bucket> \
    --acl public-read \
    --region <your AWS region> \
    --create-bucket-configuration LocationConstraint=<your AWS region>
aws s3api put-bucket-policy \
    --bucket <your executable S3 bucket> \
    --policy file://<your read policy file>
```

This is where executables generated by Github Actions will be stored. These executables will be downloaded to individual EC2 machines as they boot up.

#### Github Actions
Our Github Actions script in `.github/workflows/build.yml` automatically compiles and uploads the executables to S3. If you've forked this repo, you'd want it to upload to your own S3 bucket.

Head to your repo, click **Settings**, then **Secrets**, and add the following values:
- AWS_ACCESS_KEY_ID
- AWS_SECRET_ACCESS_KEY
- AWS_REGION
- AWS_S3_BUCKET

The first 3 values are identical to the parameters for `aws configure`.
AWS_S3_BUCKET should be set to `<your executable S3 bucket>`.

### Environment variables
The executables will not run without the correct environment variables. Before you run them, be sure to set them locally via the commands below, substituting anything `<in these brackets>` with values recorded earlier.
```shell
export ANNA_ROUTING=<your Anna ELB address>
export AWS_REGION=<your AWS region>
export AWS_AVAILABILITY_ZONE=<your AWS availability zone>
export AWS_AMI=<your AMI>
export AWS_S3_BUCKET=<your executable S3 bucket>
export PRIVATE_IP=$(curl http://169.254.169.254/latest/meta-data/local-ipv4)
export IP=$(curl http://169.254.169.254/latest/meta-data/public-ipv4)
export ANNA_KEY_PREFIX=1
export BATCH_SIZE=40
export MAX_READS_PER_SOCKET_PER_POLL=5000
```
The following parameters can be configured:
- `BATCH_SIZE`: Larger batch sizes increase throughput but increase latency if there are not enough clients.
- `MAX_READS_PER_SOCKET_PER_POLL`: The maximum number of reads that can occur per socket per poll. A larger number means fewer polls and higher performance, but runs the risk of starving other sockets if traffic is too high.
- `ANNA_KEY_PREFIX`: An arbitrary string prepended to the front of keys stored in Anna. This **MUST** be changed between executions; alternatively, Anna should be restarted, such that routing tables store up-to-date information.

## Setting up Anna on EC2
[Anna](https://github.com/hydro-project/anna) is a low-latency, eventually consistent, auto-scaling KVS acting as our system's router. Some Anna code is imported as a submodule from the [repo here](https://github.com/hydro-project/common).

The following steps are based on [instructions here](https://github.com/hydro-project/cluster/blob/master/docs/getting-started-aws.md). The scripts are not in a `.sh` file because they involve human interaction.

(Time estimate: 1 day) Before you start:
1. Buy a domain in Route53. Allow a day for it to be propagated in DNS. I followed the [instructions here](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/domain-register.html). We'll refer to this as `<your domain>` from now on.
2. Request EC2 vCPU autoscaling limit to be raised from 32. I'm on the west coast, so I modified the [EC2 settings here](https://us-west-1.console.aws.amazon.com/ec2/v2/home?region=us-west-1#Limits:), but you can change the region to your liking. You'd request an increase for `Lunning On-Demand All Standard (A, C, D, H, I, M, R, T, Z) instances`, on page 4, to at least 500. Allow a day for this limit increase to be approved.

(Time estimate: 30 minutes) Start an EC2 instance with plenty of compute power (I used a c5.2xlarge) to run the following scripts. Feel free to continue using `paxos-key.pem` from earlier for this instance.

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
sudo apt-get install -y python3-pip unzip
pip3 install awscli boto3 kubernetes
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install
```

At this point you will want to set up the AWS CLI.
```shell script
aws configure
```
Enter your access key ID, secret key, and the region you wish to use. We installed on us-west-1.

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

We need to create an S3 bucket for kops, but names can't conflict globally, so your bucket name can't be the same as mine. Substitute `<your S3 bucket>` with what you'd like to name your bucket, and `<your AWS region>` with your region. Make changes so it runs in the region you want.
```shell script
aws s3api create-bucket \
    --bucket <your S3 bucket> \
    --region <your AWS region> \
    --create-bucket-configuration LocationConstraint=<your AWS region>
aws iam create-service-linked-role --aws-service-name "elasticloadbalancing.amazonaws.com"

ssh-keygen -t rsa -N '' -f ~/.ssh/id_rsa <<< y
```

Change Anna's script to launch in your region of choice. Note that by default, Anna launches in the region `us-east-1` with the availability zone `us-east-1a`. My script changes uses the region `us-west-1` with the availability zone `us-west-1b`. Replace `<your AWS region>` and `<your AWS Availability Zone>` with values from earlier.
```shell script
cd $HYDRO_HOME/cluster

# Change us-east-1 to us-west-1
grep -rl us-east-1 . | xargs sed -i 's/us-east-1/<your AWS region>/g'
grep -rl us-west-1a . | xargs sed -i 's/<your AWS region>a/<your AWS Availability Zone>/g'

# Rerun every time if you exit & login again to this EC2 instances
export HYDRO_HOME=~/hydro-project
export AWS_ACCESS_KEY_ID=$(aws configure get aws_access_key_id)
export AWS_SECRET_ACCESS_KEY=$(aws configure get aws_secret_access_key)
```

We're almost ready to run. Set the environment variables for the domain name you bought earlier at `<your domain>`, and the same S3 bucket name you gave to kops earlier for `<your S3 bucket>`.
```shell script
# Rerun every time if you exit & login again to this EC2 instances
export HYDRO_CLUSTER_NAME=<your domain>
export KOPS_STATE_STORE=s3://<your S3 bucket>

python3 -m hydro.cluster.create_cluster -m 1 -r 1 -f 0 -s 0
```
This will run for a while, especially at the `Validating cluster...` step, so if you want to check on progress, you can open a new terminal, SSH into the same machine, and run `kops validate cluster` to check on how it's going.

**Record the routing address that this script outputs.** Our scripts will need it to talk to Anna. We'll refer to it as `<your Anna ELB address>` from now on.

We have running instances of Anna! Note that `-f 0 -s 0` means that we're not running functions or scheduling, both of which are [cloudburst](https://github.com/hydro-project/cloudburst) specific.

Do remember that
1. You cannot remove values from Anna. We store routing information in an unordered set, so if you want to clear the routing information, you'd need to restart Anna between runs. **TODO on how to do that**
2. Anna is expensive, so you might want to shut down Anna when you're not using it.

## Creating an AMI
We need EC2 instances to run components of Autoscaling Paxos.

We will use AMIs. Our executables depend upon dynamically linked libraries (such as protobuf and 0MQ), so we'd either have to `make install` them every time we boot up (which can take 10+ minutes), or package it into a system snapshot that's loaded on boot. Our goal is to scale in real time, so boot time is precious.

Follow the [instructions here](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/creating-an-ami-ebs.html#how-to-create-ebs-ami) to create an AMI. I used the [Ubuntu AMI locator](https://cloud-images.ubuntu.com/locator/ec2/) to help me find a Ubuntu 18.04 AMI with x86 and EBS backed storage in us-west-1. The ID of that AMI was `ami-00da7d550c0cbaa7b`. Once you launch that AMI from the EC2 console, clone this repo and run these scripts:
```shell script
scripts/install_dependencies_for_running.sh
scripts/download_protobuf.sh
scripts/install_protobuf.sh
```
Continue with the instructions above to create your custom AMI. Make sure to change **Shutdown behavior** to **Terminate**.
Record your custom AMI address. We will refer to it as `<your AMI>` from now on.

## Metrics
We'll use Prometheus + Grafana to scrape metrics from each of our nodes. Launch a Ubuntu EC2 node in the same region, then run the following to install both and launch Grafana:
```shell
wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee -a /etc/apt/sources.list.d/grafana.list
sudo apt-get update
sudo apt-get install -y prometheus apt-transport-https software-properties-common wget grafana

sudo service grafana-server start
```
Prometheus needs to be reconfigured to find the EC2 nodes on-the-fly. Replace `/etc/prometheus/prometheus.yml` with the following, substituting values as necessary:
```yaml
global:
  scrape_interval: 5s
  evaluation_interval: 5s

scrape_configs:
  - job_name: 'paxos'
    ec2_sd_configs:
      - region: <your AWS region>
        access_key: <AWS_ACCESS_KEY_ID>
        secret_key: <AWS_SECRET_ACCESS_KEY>
        port: 16000
        # how often we check for new nodes
        refresh_interval: 5s
    relabel_configs:
      - source_labels: [__meta_ec2_tag_Name]
        target_label: name
      # Use the instance ID as the instance label
      - source_labels: [__meta_ec2_instance_id]
        target_label: instance
```
Then run `sudo systemctl restart prometheus` for the configuration to go live.

TODO

