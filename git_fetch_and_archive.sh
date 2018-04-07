#git archive --format=tar.gz -o /home/jwalters/qemu-hpsc.tar.gz --prefix=qemu-hpsc-2.11.0/ origin/hpsc
pushd /home/jwalters/qemu_2.11/qemu-2.11
/home/jwalters/git-archive-all.sh --format tar.gz -t origin/hpsc /home/jwalters/qemu-hpsc-2.11.tar.gz
popd
mkdir -p tmp
pushd tmp
tar xf ../qemu-hpsc-2.11.tar.gz
popd
mv tmp qemu-hpsc-2.11.0
rm qemu-hpsc-2.11.tar.gz
tar czf qemu-hpsc.tar.gz qemu-hpsc-2.11.0
