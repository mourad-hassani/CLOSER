from gym.spaces import MultiDiscrete

action_space = MultiDiscrete([101 for _ in range(2)])

print(action_space.nvec)

nb_actions = 1
for i in range(len(action_space.nvec)):
    nb_actions *= action_space.nvec[i]

print(nb_actions)